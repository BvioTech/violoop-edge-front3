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
    case MascotDirector::Working:
        break; // not looked up by key, see conditionClip()
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

void MascotDirector::cue(Cue cue)
{
    // The goal is over whether or not anybody gets to see the confetti.
    if (cue == Done)
        m_workClip.clear();
    if (!m_attached)
        return;
    if (m_mode == Reacting && !m_interruptible)
        return;
    if (cue == Done) {
        if (!m_conditions.isEmpty() && m_conditions.firstKey() < Working)
            return;
    } else if (!m_conditions.isEmpty()) {
        return;
    }

    const QStringList clips = m_catalog.cueClips.value(cueKey(cue));
    if (clips.isEmpty())
        return;
    const QString clip = cue == TouchMiddle ? clips.at(m_touchMiddleCount++ % clips.size()) : clips.first();
    m_interruptible = cue == Welcome;
    start(Reacting, clip, false, true);
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
    m_pausedRemaining = -1;
}

void MascotDirector::stagePaused(bool paused)
{
    if (paused) {
        if (m_timer.isActive()) {
            m_pausedRemaining = m_timer.remainingTime();
            m_timer.stop();
        }
    } else if (m_pausedRemaining >= 0) {
        m_timer.start(m_pausedRemaining);
        m_pausedRemaining = -1;
    }
}

// The one place where the clip is decided. Called after anything that
// could change the answer; does nothing when the answer is what is
// already playing.
void MascotDirector::resolve()
{
    if (!m_attached || m_catalog.idleBase.isEmpty())
        return;
    if (m_mode == Reacting && !m_interruptible)
        return;

    if (!m_conditions.isEmpty()) {
        const Condition top = m_conditions.firstKey();
        const QString clip = conditionClip(top);
        if (m_mode == FollowingCondition && clip == m_clip)
            return;
        start(FollowingCondition, clip, top == Manual || !m_catalog.once.contains(clip), false);
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
    // for), just carries on.
    const bool carriesOn = loop && m_looping && clip == m_clip;
    m_mode = mode;
    m_clip = clip;
    m_looping = loop;
    m_pausedRemaining = -1;
    if (timed)
        m_timer.start(m_durations.value(clip));
    else
        m_timer.stop();
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
    } else if (m_mode == Idling) {
        m_inInterlude = !m_inInterlude;
        start(Idling, m_inInterlude ? nextInterlude() : m_catalog.idleBase, true, true);
    }
}

QString MascotDirector::conditionClip(Condition condition)
{
    QString clip;
    if (condition == Manual)
        clip = m_conditions.value(Manual);
    else if (condition == Working)
        clip = workClip(m_conditions.value(Working));
    else
        clip = m_catalog.conditionClips.value(conditionKey(condition));
    // A model without a clip for this shows its resting clip instead.
    return m_durations.contains(clip) ? clip : m_catalog.idleBase;
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
