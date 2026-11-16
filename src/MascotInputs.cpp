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
            emit snapshotWanted(authorizationPath());
        if (m_goalActive)
            emit snapshotWanted(goalPath());
        if (m_conversing)
            emit snapshotWanted(conversationPath());
    });
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
    emit snapshotWanted(authorizationPath());
    emit snapshotWanted(goalPath());
    emit snapshotWanted(conversationPath());
}

void MascotInputs::eventReceived(const QString &type, const QString &data)
{
    if (type == QLatin1String("notification")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (event.value(QLatin1String("kind")).toString() != QLatin1String("authorization"))
            return;
        // A new request shows at once; anything else, and also whether
        // the new one is still there, is for the snapshot to say.
        if (event.value(QLatin1String("state")).toString() == QLatin1String("open"))
            setWaiting(true, event.value(QLatin1String("notifId")).toString());
        emit snapshotWanted(authorizationPath());
    } else if (type == QLatin1String("goal.state")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (event.isEmpty())
            return;
        const bool done = event.value(QLatin1String("state")).toString() == QLatin1String("done");
        // Before the first snapshot there is nothing to compare `last`
        // with: then only a `done` event is known to be news. (And a
        // violoop-edge that does not send `last` yet has only the event.)
        const bool known = m_lastGoalKnown;
        bool celebrate = noteLastGoal(event.value(QLatin1String("last")).toObject());
        if (!known || !event.contains(QLatin1String("last")))
            celebrate = done;
        // The cue first: it ends the goal that was being shown, and what
        // `active` says then only takes over once the confetti is down.
        if (celebrate)
            m_director->cue(MascotDirector::Done);
        setGoal(event.value(QLatin1String("active")).toObject());
    } else if (type == QLatin1String("realtime:state")) {
        const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
        if (!event.isEmpty())
            setConversation(event.value(QLatin1String("state")).toString());
    }
}

void MascotInputs::snapshotReceived(const QString &path, int statusCode, const QString &body)
{
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
        if (statusCode == 200 && noteLastGoal(reply.value(QLatin1String("last")).toObject()) && known)
            m_director->cue(MascotDirector::Done);
        setGoal(active);
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
void MascotInputs::pinConversation(const QString &state)
{
    m_conversationPinned = false;
    setConversation(state);
    m_conversationPinned = true;
    m_conversing = false; // nothing to ask the backend about
    updatePoll();
}

void MascotInputs::setConversation(const QString &state)
{
    if (m_conversationPinned)
        return;
    m_conversing = !state.isEmpty() && state != QLatin1String("ended");
    if (!m_conversing) {
        m_director->clear(MascotDirector::Conversation);
    } else {
        QString phase = QStringLiteral("preparing");
        if (state == QLatin1String("listening") || state == QLatin1String("speaking"))
            phase = state;
        else if (state == QLatin1String("thinking") || state == QLatin1String("working"))
            phase = QStringLiteral("thinking");
        m_director->set(MascotDirector::Conversation, phase);
    }
    updatePoll();
}

// `last`: as in GET /status/goal - empty, or the goal that ended last.
// Remembers it; true when it is one that was completed and that has not
// been seen before.
bool MascotInputs::noteLastGoal(const QJsonObject &last)
{
    m_lastGoalKnown = true;
    if (last.isEmpty())
        return false;
    const QString key = last.value(QLatin1String("goalId")).toString() + QLatin1Char('\n')
        + QString::number(last.value(QLatin1String("time")).toDouble(), 'f', 0);
    if (key == m_lastGoal)
        return false;
    m_lastGoal = key;
    return last.value(QLatin1String("state")).toString() == QLatin1String("done");
}

void MascotInputs::updatePoll()
{
    const bool wanted = m_waiting || m_goalActive || m_conversing;
    if (wanted && !m_poll.isActive())
        m_poll.start();
    else if (!wanted)
        m_poll.stop();
}
