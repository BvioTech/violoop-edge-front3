#include "MascotInputs.h"

#include <QJsonDocument>

#include "MascotDirector.h"

namespace {
// Failed cloud probes in a row that make the device offline.
constexpr int kOfflineAfterFailedProbes = 2;
}

MascotInputs::MascotInputs(MascotDirector *director, int pollMs, QObject *parent)
    : QObject(parent)
    , m_director(director)
{
    m_poll.setInterval(pollMs);
    connect(&m_poll, &QTimer::timeout, this, [this]() {
        if (m_waiting)
            ask(authorizationPath());
        if (m_goalActive)
            ask(goalPath());
        if (m_conversing)
            ask(conversationPath());
    });
    m_celebrationRetry.setInterval(1000);
    connect(&m_celebrationRetry, &QTimer::timeout, this, [this]() { celebrate(false); });
}

void MascotInputs::setCelebrationRetry(int forMs, int everyMs)
{
    m_celebrationForMs = forMs;
    m_celebrationRetry.setInterval(everyMs);
}

void MascotInputs::ask(const QString &path)
{
    ++m_asked[path];
    emit snapshotWanted(path);
}

// An event about what the snapshot at `path` shows: whatever answers are
// still on their way are older than it.
void MascotInputs::overtaken(const QString &path)
{
    if (m_asked.value(path) > 0)
        m_overtaken.insert(path);
}

void MascotInputs::connectivityProbed(bool ok)
{
    if (ok) {
        m_failedProbes = 0;
        m_director->clear(MascotDirector::Offline);
    } else if (++m_failedProbes >= kOfflineAfterFailedProbes) {
        m_failedProbes = kOfflineAfterFailedProbes; // no overflow over a long outage
        m_director->set(MascotDirector::Offline);
    }
}

void MascotInputs::eventsConnected()
{
    // Whatever happened while the stream was down was not replayed.
    ask(authorizationPath());
    ask(goalPath());
    ask(conversationPath());
}

void MascotInputs::eventReceived(const QString &type, const QString &data)
{
    if (type == QLatin1String("notification")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (event.value(QLatin1String("kind")).toString() != QLatin1String("authorization"))
            return;
        // A new request shows at once; anything else, and also whether
        // the new one is still there, is for the snapshot to say.
        overtaken(authorizationPath());
        if (event.value(QLatin1String("state")).toString() == QLatin1String("open"))
            setWaiting(true, event.value(QLatin1String("notifId")).toString());
        ask(authorizationPath());
    } else if (type == QLatin1String("goal.state")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (event.isEmpty())
            return;
        overtaken(goalPath());
        const bool done = event.value(QLatin1String("state")).toString() == QLatin1String("done");
        // Before the first snapshot there is nothing to compare `last`
        // with: then only a `done` event is known to be news. (And a
        // violoop-edge that does not send `last` yet has only the event.)
        const bool known = m_lastGoalKnown;
        bool news = noteLastGoal(event.value(QLatin1String("last")).toObject());
        if (!known || !event.contains(QLatin1String("last")))
            news = done;
        // `active` first: with another goal running already, the director
        // then knows that it is not that one which is done.
        setGoal(event.value(QLatin1String("active")).toObject());
        if (news)
            celebrate(true);
    } else if (type == QLatin1String("realtime:state")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (event.isEmpty())
            return;
        overtaken(conversationPath());
        setConversation(event.value(QLatin1String("state")).toString());
    }
}

void MascotInputs::snapshotReceived(const QString &path, int statusCode, const QString &body)
{
    if (path != authorizationPath() && path != goalPath() && path != conversationPath())
        return;
    if (m_asked[path] > 0)
        --m_asked[path];
    if (m_overtaken.contains(path)) {
        // Older than what an event has said since. Once the last such
        // answer is in, ask afresh.
        if (m_asked[path] == 0) {
            m_overtaken.remove(path);
            ask(path);
        }
        return;
    }

    // No answer, or an error (503 while violoop-edge starts): it is down
    // or was restarted, and neither its authorization requests nor its
    // goals nor a voice session survive that.
    QJsonObject reply;
    if (statusCode == 200)
        reply = QJsonDocument::fromJson(body.toUtf8()).object();
    if (path == conversationPath()) {
        // Without a session: {"mode":"idle"}, no `state`.
        setConversation(reply.value(QLatin1String("state")).toString());
        return;
    }
    const QJsonObject active = reply.value(QLatin1String("active")).toObject();
    if (path == authorizationPath())
        setWaiting(!active.isEmpty(), active.value(QLatin1String("context")).toString());
    else if (path == goalPath()) {
        // The first snapshot only says where things stand.
        const bool known = m_lastGoalKnown;
        const bool news = statusCode == 200 && noteLastGoal(reply.value(QLatin1String("last")).toObject());
        setGoal(active);
        if (news && known)
            celebrate(true);
    }
}

void MascotInputs::setWaiting(bool waiting, const QString &request)
{
    m_waiting = waiting;
    if (waiting)
        m_director->set(MascotDirector::WaitingUser, request);
    else
        m_director->clear(MascotDirector::WaitingUser);
    updatePoll();
}

// `active`: as in GET /status/goal - empty, or a goal that is being
// worked on, or one that failed.
void MascotInputs::setGoal(const QJsonObject &active)
{
    const QString state = active.value(QLatin1String("state")).toString();
    const bool working = state == QLatin1String("working");
    const bool failed = state == QLatin1String("failed");
    m_goalActive = working || failed;
    if (working)
        m_director->set(MascotDirector::Working, active.value(QLatin1String("goalId")).toString());
    else
        m_director->clear(MascotDirector::Working);
    if (failed)
        m_director->set(MascotDirector::Error);
    else
        m_director->clear(MascotDirector::Error);
    updatePoll();
}

// `state`: a phase of violoop-edge's realtime voice session, or empty or
// "ended" for none.
// `last`: as in GET /status/goal - empty, or the goal that ended last.
// Remembers it; true when it is one that was completed and that has not
// been seen before. (More than one is remembered: an answer that is late
// can still name the one before.)
bool MascotInputs::noteLastGoal(const QJsonObject &last)
{
    m_lastGoalKnown = true;
    if (last.isEmpty())
        return false;
    const QString key = last.value(QLatin1String("goalId")).toString() + QLatin1Char('\n')
        + QString::number(last.value(QLatin1String("time")).toDouble(), 'f', 0);
    if (m_endedGoals.contains(key))
        return false;
    m_endedGoals.append(key);
    if (m_endedGoals.size() > 16)
        m_endedGoals.removeFirst();
    return last.value(QLatin1String("state")).toString() == QLatin1String("done");
}

// Throws the confetti, or keeps offering it for a while when the director
// has no use for it right now. `fresh`: for a goal just completed, as
// opposed to offering again what is still owed.
void MascotInputs::celebrate(bool fresh)
{
    if (fresh)
        m_celebrationDeadline.setRemainingTime(m_celebrationForMs);
    if (m_director->cue(MascotDirector::Done) || m_celebrationDeadline.hasExpired())
        m_celebrationRetry.stop();
    else if (!m_celebrationRetry.isActive())
        m_celebrationRetry.start();
}

void MascotInputs::pinConversation(const QString &state)
{
    m_conversationPinned = true;
    m_conversing = false; // nothing to ask the backend about
    showConversation(state);
    updatePoll();
}

// `state`: a phase of violoop-edge's realtime voice session, or empty or
// "ended" for none.
void MascotInputs::setConversation(const QString &state)
{
    if (m_conversationPinned)
        return;
    m_conversing = !state.isEmpty() && state != QLatin1String("ended");
    showConversation(state);
    updatePoll();
}

void MascotInputs::showConversation(const QString &state)
{
    if (state.isEmpty() || state == QLatin1String("ended")) {
        m_director->clear(MascotDirector::Conversation);
        return;
    }
    // A phase this does not know is shown like the start of a session.
    QString phase = QStringLiteral("preparing");
    if (state == QLatin1String("listening") || state == QLatin1String("speaking"))
        phase = state;
    else if (state == QLatin1String("thinking") || state == QLatin1String("working"))
        phase = QStringLiteral("thinking");
    m_director->set(MascotDirector::Conversation, phase);
}

void MascotInputs::updatePoll()
{
    const bool wanted = m_waiting || m_goalActive || m_conversing;
    if (wanted && !m_poll.isActive())
        m_poll.start();
    else if (!wanted)
        m_poll.stop();
}
