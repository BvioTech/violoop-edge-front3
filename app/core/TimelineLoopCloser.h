#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QVariant>

// Makes the glTF animation that Quick3D's RuntimeLoader auto-plays loop
// seamlessly. RuntimeLoader turns a glTF animation into a QtQuickTimeline
// Timeline running 0..length on an infinite loop, but copies the glTF
// keyframe times as-is - and a glTF clip's first keyframe usually isn't at
// t=0 (CesiumMan.glb: 48 keys at 1/24s..2s, so the first one lands on
// frame 41.67 of 2000). For the frames before a group's first keyframe,
// QQuickKeyframeGroup::evaluate() interpolates from the property's
// *pre-animation* value (the node's rest/bind transform), not from the
// end of the previous loop. Net effect: at every wrap-around each bone
// snaps to its rest pose and blends back into the walk cycle over ~42ms -
// a visible twitch every 2s, i.e. the clip never plays as a closed loop
// the way other glTF viewers show it.
//
// The Timeline is created in C++ as a plain QObject child of the imported
// scene root, unreachable from QML (not a Quick3D child, no id), hence
// this helper: it finds the generated Timeline under `sceneRoot` and gives
// every keyframe group lacking a keyframe at the timeline's start frame
// one there, carrying the group's *last* keyframe value. The wrap then
// becomes last pose -> (same pose at frame 0) -> first keyframe, continuous
// in both position and time, and the loop period stays the clip's own
// length.
//
// Works through QObject properties/QQmlListReference only, so the app
// doesn't need QtQuickTimeline's private headers.
class TimelineLoopCloser : public QObject
{
    Q_OBJECT

public:
    explicit TimelineLoopCloser(QObject *parent = nullptr);

    // Call once a RuntimeLoader reaches RuntimeLoader.Success, passing the
    // loader itself. Returns the number of keyframe groups patched (0 if
    // there's no animation, or every group already starts at frame 0 -
    // calling it twice is harmless).
    Q_INVOKABLE int closeLoops(QObject *sceneRoot);

    // Pauses/resumes the TimelineAnimation(s) RuntimeLoader generated
    // under `sceneRoot` (same unreachable-from-QML objects as above, which
    // is why this lives here despite the class name). Returns how many
    // animations were found.
    Q_INVOKABLE int setPaused(QObject *sceneRoot, bool paused);

    // --- Multi-clip models -------------------------------------------
    // A glTF with several animations (VIO_DesktopPet.glb: 19, named by
    // event - "idle.default", "welcome", "touch.left", ...) comes out of
    // RuntimeLoader as one Timeline + TimelineAnimation per clip. Measured
    // on dev1 (Qt 6.8.5-violoop3): only the *first* Timeline is enabled,
    // so only the first clip is visible, but all the TimelineAnimations
    // run (the other clips' currentFrame ticks along to no effect). The
    // clip's glTF name survives as the TimelineAnimation's objectName;
    // node names do not survive anywhere.

    // The clips' names in file order ("clip <n>" for an unnamed one).
    Q_INVOKABLE QStringList clipNames(QObject *sceneRoot);

    // Makes `name` the one clip that plays, looping from its first frame,
    // and stops the others' animations. Disabling a Timeline puts every
    // property it animated back to its pre-animation value, and enabling
    // one captures the current values as *its* pre-animation values - so
    // the old clip is always disabled before the new one is enabled,
    // otherwise the new clip would capture (and later restore) the old
    // clip's mid-animation pose as the rest pose. That reset is also what
    // makes clips that hide/show props by scale (this model parks unused
    // props at scale 1e-5) not leak props into the next clip. Returns
    // false if there is no clip called `name`; emits clipStarted().
    //
    // fadeMs > 0 cross-fades instead of cutting: QtQuickTimeline cannot
    // blend two timelines, so the pose on screen at the moment of the
    // switch is captured, and for the first fadeMs of the new clip every
    // position/rotation/scale either clip animates is written as a blend
    // from that captured value to whatever the new clip just set (or to
    // the rest value, for properties only the old clip animated), right
    // after the new timeline evaluates each frame. The blend's progress
    // is the new clip's own frame counter, so pausing pauses the fade.
    // Hidden<->shown scale switches (see restoreHolds) are left out:
    // props and lamp colours still cut, only the motion is smoothed.
    Q_INVOKABLE bool playClip(QObject *sceneRoot, const QString &name, int fadeMs = 0);

    // Like playClip(), but lets the clip that is playing finish its
    // current loop first and switches at the wrap-around. For clips that
    // start and end in the same pose (17 of VIO_DesktopPet.glb's 19 start
    // from the rest pose) that is seamless even without a fade. A later
    // playClip()/playClipAtLoopEnd() replaces a switch still waiting.
    Q_INVOKABLE bool playClipAtLoopEnd(QObject *sceneRoot, const QString &name, int fadeMs = 0);

    // Undoes the importer's keyframe thinning where it changes the motion.
    // The importer drops every keyframe whose value equals the previous
    // kept one, including the *last* key of a run of identical values -
    // and that last key is what made the run a hold. Without it the
    // timeline interpolates linearly across the whole run: the model's
    // blink (eye scale 1 for 3.6s, then a 0.2s squash) turned into the
    // eyes squashing slowly for 3.6s, and props that pop in by scale would
    // inflate gradually instead. For every gap between two kept keys
    // longer than the timeline's sampling interval this re-inserts the
    // hold's end (the earlier key's value, one interval before the later
    // key).
    //
    // Hidden<->shown switches get stricter treatment. The model swaps
    // meshes by scaling one node to 1e-5 and another to 1 (its rim light
    // is 18 lamp meshes, one per brightness level, of which one is shown
    // at a time), with glTF STEP interpolation and a guard key 2.5ms
    // before each switch. The importer ignores STEP and drops the guard
    // key, and re-inserting the hold one sampling interval early still
    // left a 33ms ramp in which the outgoing and incoming lamps were both
    // shrunk to nothing-in-particular: the rim light went dark for a
    // frame or two at every brightness step (seen on dev1's panel as the
    // light flickering). So any scale segment with exactly one end
    // collapsed holds until 0.01ms before its end, dropped keys or not.
    //
    // The sampling interval is inferred as the smallest gap between two
    // keys anywhere in that timeline, which is exact for clips baked at a
    // fixed rate (both models we have: 30 and 24 fps). A clip that mixes
    // densely and sparsely keyed channels would have its sparse channels
    // turned from ramps into holds - the real fix belongs in the importer
    // (our Qt fork), which still has the original key times. Call once
    // after load, like closeLoops(); returns the number of keys inserted
    // (0 on a second call).
    Q_INVOKABLE int restoreHolds(QObject *sceneRoot);

signals:
    // `name` has just become the playing clip of `sceneRoot`.
    void clipStarted(QObject *sceneRoot, const QString &name);

private slots:
    void onFadeFrame();
    void onQueuedFrame();
    void startQueued();

private:
    // One property being cross-faded.
    struct Faded
    {
        QPointer<QObject> target;
        QByteArray property;
        QVariant from;          // on screen when the switch happened
        bool animatedByNew;     // new clip writes it every frame
        QVariant to;            // otherwise: where it came to rest
    };
    void cancelFade();
    void cancelQueued();

    QPointer<QObject> m_fadeTimeline;
    QList<Faded> m_faded;
    int m_fadeMs = 0;

    QPointer<QObject> m_queuedOn;   // timeline whose wrap-around we wait for
    QPointer<QObject> m_queuedScene;
    QString m_queuedName;
    int m_queuedFadeMs = 0;
    qreal m_queuedLastFrame = 0;
};
