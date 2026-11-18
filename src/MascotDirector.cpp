#include "MascotDirector.h"

#include <QDebug>
#include <QRandomGenerator>

namespace {

QString conditionKey(MascotDirector::Condition condition)
{
    switch (condition) {
    case MascotDirector::Offline: return QStringLiteral("offline");
    case MascotDirector::Error: return QStringLiteral("error");
    case MascotDirector::WaitingUser: return QStringLiteral("waiting_user");
    case MascotDirector::Manual:
    case MascotDirector::Conversation:
    case MascotDirector::Working:
        break; // not looked up by key, see conditionChoice()
    }
    return QString();
}

QString cueKey(MascotDirector::Cue cue)
{
    switch (cue) {
    case MascotDirector::Welcome: return QStringLiteral("welcome");
    case MascotDirector::Done: return QStringLiteral("done");
    case MascotDirector::TouchLeft: return QStringLiteral("touch_left");
    case MascotDirector::TouchMiddle: return QStringLiteral("touch_middle");
    case MascotDirector::TouchRight: return QStringLiteral("touch_right");
    case MascotDirector::TouchUpperLeft: return QStringLiteral("touch_upper_left");
    case MascotDirector::TouchUpperMiddle: return QStringLiteral("touch_upper_middle");
    case MascotDirector::TouchUpperRight: return QStringLiteral("touch_upper_right");
    case MascotDirector::TouchLowerMiddle: return QStringLiteral("touch_lower_middle");
    }
    return QString();
}

} // namespace

MascotDirector::MascotDirector(const QJsonObject &manifest, QObject *parent)
    : QObject(parent)
    , m_manifest(manifest)
    , m_picker([](int n) { return int(QRandomGenerator::global()->bounded(n)); })
{
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &MascotDirector::onTimeout);
}

void MascotDirector::set(Condition condition, const QString &context)
{
    const auto it = m_conditions.constFind(condition);
    if (it != m_conditions.constEnd() && it.value() == context)
        return;
    m_conditions.insert(condition, context);
    resolve();
}

void MascotDirector::clear(Condition condition)
{
    if (m_conditions.remove(condition) > 0)
        resolve();
}

bool MascotDirector::cue(Cue cue)
{
    // The goal is over whether or not anybody gets to see the confetti.
    if (cue == Done && !m_conditions.contains(Working))
        m_workClip.clear();
    if (!m_attached)
        return false;
    if (m_mode == Reacting && !m_interruptible)
        return false;
    if (cue == Done) {
        if (!m_conditions.isEmpty() && m_conditions.firstKey() < Working)
            return false;
    } else if (!m_conditions.isEmpty()) {
        return false;
    }

    const QStringList clips = m_catalog.cueClips.value(cueKey(cue));
    if (clips.isEmpty())
        return false;
    const QString clip = cue == TouchMiddle ? clips.at(m_touchMiddleCount++ % clips.size()) : clips.first();
    m_interruptible = cue == Welcome;
    start(Reacting, clip, false, true);
    return true;
}

void MascotDirector::stageAttached(const QStringList &clips, const QVariantList &durationsMs)
{
    m_durations.clear();
    for (qsizetype i = 0; i < clips.size(); ++i)
        m_durations.insert(clips.at(i), i < durationsMs.size() ? durationsMs.at(i).toInt() : 0);
    m_catalog = ActionCatalog::fromJson(m_manifest, clips);
    m_attached = true;
    m_mode = Stopped;
    m_clip.clear();
    m_looping = false;
    resolve();
}

void MascotDirector::stageDetached()
{
    m_attached = false;
    m_mode = Stopped;
    m_clip.clear();
    m_timer.stop();
    resolve();
}

// The one place where the clip is decided. Called after anything that
// could change the answer; does nothing when the answer is what is
// already playing.
void MascotDirector::resolve()
{
    resolveClip();
    const bool steady = m_mode == FollowingCondition && !m_conditions.isEmpty()
        && m_conditions.firstKey() == Conversation
        && m_conditions.first() == QLatin1String("listening");
    if (steady != m_steadyLamp) {
        m_steadyLamp = steady;
        emit steadyLampChanged();
    }
}

void MascotDirector::resolveClip()
{
    if (!m_attached || m_catalog.idleBase.isEmpty())
        return;
    if (m_mode == Reacting && !m_interruptible)
        return;

    if (!m_conditions.isEmpty()) {
        const Condition top = m_conditions.firstKey();
        const Choice choice = conditionChoice(top);
        const QString following = choice.clip + QLatin1Char('\n') + choice.after;
        if (m_mode == FollowingCondition && following == m_following)
            return;
        m_following = following;
        m_after = choice.after;
        start(FollowingCondition, choice.clip, choice.loop, !choice.after.isEmpty());
        return;
    }
    if (m_mode == Reacting || m_mode == Idling)
        return;
    m_inInterlude = false;
    start(Idling, m_catalog.idleBase, true, !m_catalog.idleInterludes.isEmpty());
}

void MascotDirector::start(Mode mode, const QString &clip, bool loop, bool timed)
{
    // A looping clip that is asked for again, for another reason (the
    // resting clip standing in for a condition the model has no clip
    // for), just carries on. Not when it is about to be timed: a round
    // is counted from the clip's start.
    const bool carriesOn = loop && m_looping && clip == m_clip && !timed;
    m_mode = mode;
    m_clip = clip;
    m_looping = loop;
    if (timed) {
        // A clip too short to time (a single pose is 0 ms long) must not
        // turn this into a busy loop of directives.
        int duration = m_durations.value(clip);
        if (duration < fadeMs)
            duration = mode == Idling ? poseRoundMs : fadeMs;
        m_timer.start(duration);
    } else {
        m_timer.stop();
    }
    if (carriesOn)
        return;
    qDebug() << "MascotDirector:" << clip << (loop ? "(loop)" : "(once)");
    emit directive(clip, loop, fadeMs);
}

// A once-only clip or an idle round is over.
void MascotDirector::onTimeout()
{
    if (m_mode == Reacting) {
        m_mode = Stopped;
        resolve();
    } else if (m_mode == FollowingCondition && !m_after.isEmpty()) {
        const QString next = m_after;
        m_after.clear();
        start(FollowingCondition, next, true, false);
    } else if (m_mode == Idling) {
        m_inInterlude = !m_inInterlude;
        start(Idling, m_inInterlude ? nextInterlude() : m_catalog.idleBase, true, true);
    }
}

MascotDirector::Choice MascotDirector::conditionChoice(Condition condition)
{
    Choice choice;
    if (condition == Manual) {
        choice.clip = m_conditions.value(Manual);
    } else if (condition == Working) {
        choice.clip = workClip(m_conditions.value(Working));
    } else if (condition == Conversation) {
        const QString phase = m_conditions.value(Conversation);
        if (phase == QLatin1String("listening")) {
            choice.clip = m_catalog.conditionClips.value(conditionKey(WaitingUser));
        } else if (phase == QLatin1String("thinking")) {
            // Without a goal: the clip the next goal will get, so that
            // nothing changes when the thinking turns into one.
            if (m_conditions.contains(Working))
                choice.clip = workClip(m_conditions.value(Working));
            else if (!m_catalog.workClips.isEmpty())
                choice.clip = m_catalog.workClips.at(m_workCursor % m_catalog.workClips.size());
        } else if (phase == QLatin1String("speaking")) {
            const QString nod = m_catalog.cueClips.value(cueKey(Welcome)).value(0);
            if (m_durations.contains(nod)) {
                choice.clip = nod;
                choice.loop = false;
                choice.after = m_catalog.idleBase;
            }
        }
        if (choice.clip.isEmpty())
            choice.clip = m_catalog.idleBase;
    } else {
        choice.clip = m_catalog.conditionClips.value(conditionKey(condition));
    }
    // A clip marked as once-only plays once and holds its last frame -
    // except one picked by hand, and the nod, which has its own sequel.
    if (condition != Manual && choice.after.isEmpty())
        choice.loop = !m_catalog.once.contains(choice.clip);
    // A model without a clip for this shows its resting clip instead.
    if (!m_durations.contains(choice.clip))
        choice = { m_catalog.idleBase, true, QString() };
    return choice;
}

QString MascotDirector::workClip(const QString &goal)
{
    if (m_catalog.workClips.isEmpty())
        return QString();
    if (m_workClip.isEmpty() || goal != m_goal || !m_catalog.workClips.contains(m_workClip)) {
        m_workClip = m_catalog.workClips.at(m_workCursor++ % m_catalog.workClips.size());
        m_goal = goal;
    }
    return m_workClip;
}

// Any interlude but the one that played last.
QString MascotDirector::nextInterlude()
{
    QStringList choices = m_catalog.idleInterludes;
    if (choices.size() > 1)
        choices.removeAll(m_lastInterlude);
    m_lastInterlude = choices.at(qBound(0, m_picker(int(choices.size())), int(choices.size()) - 1));
    return m_lastInterlude;
}
