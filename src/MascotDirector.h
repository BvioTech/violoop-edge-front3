#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <functional>

#include "ActionCatalog.h"

// Decides which of the mascot's animation clips should be playing. It
// knows nothing about where its input comes from (backend events, touch,
// buttons) nor about what plays the clips (ClipPlayer, by way of
// qml/HomePage.qml): input arrives through set()/clear()/cue(), the
// decision leaves through the directive() signal.
//
// Two kinds of input:
//
//   - a Condition is something that lasts ("working", "offline"). Whoever
//     knows about it sets it and clears it; of the ones set, the one
//     highest up in the enum decides the clip. With none set the mascot
//     idles: the resting clip for one round, a random small thing for one
//     round, and so on.
//   - a Cue is a moment ("done", "touched on the left"). Its clip plays
//     once, and then whatever the conditions say *by then* takes over -
//     nothing is remembered from before the cue.
//
// One app-wide instance (main.cpp, `mascot` in QML), at work whether or
// not the home page is showing: conditions are tracked all the time, so
// that the page shows the present state the moment it comes back. Cues
// that arrive while the mascot is not on screen are dropped.
//
// The director does its own timing (when a once-only clip is over, when
// an idle round is over), from the clip lengths the page reports in
// stageAttached(). The clips run on the same wall clock, so the two agree
// to within a frame or so, which the cross-fade covers.
class MascotDirector : public QObject
{
    Q_OBJECT

public:
    // In order of precedence, highest first.
    enum Condition {
        // Somebody picked a clip by hand (the home page's buttons); the
        // context is the clip's name. It loops, whatever its kind.
        Manual,
        Offline,
        Error,
        WaitingUser,
        // The context names the goal being worked on. Each new goal gets
        // the next of the catalog's work clips and keeps it for as long
        // as it is the goal, also across interruptions by the conditions
        // above.
        Working,
    };
    Q_ENUM(Condition)

    enum Cue {
        // Only while idle; a condition coming up cuts it short.
        Welcome,
        // Plays in full. Dropped while something above Working is set
        // (no celebrating while offline). Ends the current goal.
        Done,
        // Only while idle; play in full.
        TouchLeft,
        TouchMiddle,
        TouchRight,
    };
    Q_ENUM(Cue)

    // `manifest`: see ActionCatalog.h.
    explicit MascotDirector(const QJsonObject &manifest, QObject *parent = nullptr);

    Q_INVOKABLE void set(MascotDirector::Condition condition, const QString &context = QString());
    Q_INVOKABLE void clear(MascotDirector::Condition condition);
    Q_INVOKABLE void cue(MascotDirector::Cue cue);

    // The page that shows the mascot reports in: the loaded model's clips
    // in file order and their lengths in milliseconds. The current
    // decision is sent out again, from the start.
    Q_INVOKABLE void stageAttached(const QStringList &clips, const QVariantList &durationsMs);
    Q_INVOKABLE void stageDetached();
    // The page froze or unfroze the playing clip; the director's clock
    // stops and resumes with it.
    Q_INVOKABLE void stagePaused(bool paused);

    // Picks one of n idle interludes, returning 0..n-1. Random by
    // default; tests put in something predictable.
    void setPicker(std::function<int(int)> picker) { m_picker = std::move(picker); }

    // Length of the cross-fade every directive asks for.
    static constexpr int fadeMs = 225;
    // How long an idle round lasts when its clip is shorter than the
    // cross-fade, i.e. is a pose rather than a motion.
    static constexpr int poseRoundMs = 4000;

signals:
    // Play `clip` now, looping or once (then holding its last frame),
    // cross-faded over fadeMs from whatever is on screen.
    void directive(const QString &clip, bool loop, int fadeMs);

private:
    enum Mode { Stopped, FollowingCondition, Reacting, Idling };

    void resolve();
    void start(Mode mode, const QString &clip, bool loop, bool timed);
    void onTimeout();
    QString conditionClip(Condition condition);
    QString workClip(const QString &goal);
    QString nextInterlude();

    QJsonObject m_manifest;
    ActionCatalog m_catalog;
    QHash<QString, int> m_durations;
    bool m_attached = false;

    QMap<Condition, QString> m_conditions;

    Mode m_mode = Stopped;
    QString m_clip;
    bool m_looping = false;
    // Reacting: whether a condition may cut the reaction short.
    bool m_interruptible = false;
    // Idling: whether an interlude, not the resting clip, is playing.
    bool m_inInterlude = false;
    QString m_lastInterlude;
    int m_touchMiddleCount = 0;

    // The goal being worked on and the clip it was given; the clip is
    // empty once the goal is done.
    QString m_goal;
    QString m_workClip;
    int m_workCursor = 0;

    QTimer m_timer;
    bool m_paused = false;
    // Left on the clock when the stage was paused, -1 when not paused or
    // nothing was being timed.
    int m_pausedRemaining = -1;
    std::function<int(int)> m_picker;
};
