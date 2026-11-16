#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariant>

// Plays the animation clips of one model loaded by Quick3D's
// RuntimeLoader: which clips there are, which one is playing, switching
// between them, pausing - and repairing what the importer gets wrong
// about them on the way in. A QML type (import EdgeFrontQt.Core,
// registered in main.cpp), one per loaded model:
//
//   ClipPlayer { id: clips }
//   RuntimeLoader {
//       onStatusChanged: status === RuntimeLoader.Success ? clips.attach(this)
//                                                         : clips.detach()
//   }
//
// It has to be C++: RuntimeLoader turns each glTF animation into a
// QtQuickTimeline Timeline + TimelineAnimation created as plain QObject
// children of the imported scene root, unreachable from QML (not Quick3D
// children, no ids). They are found and driven here through QObject
// properties and QQmlListReference only, so the app doesn't need
// QtQuickTimeline's private headers.
//
// What a model with several clips looks like (VIO_DesktopPet.glb: 19,
// named by event - "idle.default", "welcome", "touch.left", ...),
// measured on dev1 (Qt 6.8.5-violoop3): one Timeline + TimelineAnimation
// per clip, only the *first* Timeline enabled, so only the first clip is
// visible, but all the TimelineAnimations running (the other clips'
// currentFrame ticks along to no effect). The clip's glTF name survives
// as the TimelineAnimation's objectName; node names do not survive
// anywhere.
class ClipPlayer : public QObject
{
    Q_OBJECT
    // The clips' names in file order ("clip <n>" for an unnamed one).
    // Empty while no model is attached.
    Q_PROPERTY(QStringList clipNames READ clipNames NOTIFY clipNamesChanged)
    // The clip that is playing; change it with play().
    Q_PROPERTY(QString currentClip READ currentClip NOTIFY currentClipChanged)
    // Freezes the playing clip where it is. Starting a clip clears it.
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
    explicit ClipPlayer(QObject *parent = nullptr);

    QStringList clipNames() const { return m_clipNames; }
    QString currentClip() const { return m_currentClip; }
    bool paused() const { return m_paused; }
    void setPaused(bool paused);

    // Takes over the clips of the model under `sceneRoot`. Call once a
    // RuntimeLoader reaches RuntimeLoader.Success, passing the loader
    // itself. Repairs the clips (closeLoops, restoreHolds - below) and
    // starts the first one: through play() even though RuntimeLoader
    // already started it, so the other clips' animations stop ticking in
    // the background.
    Q_INVOKABLE void attach(QObject *sceneRoot);

    // Forgets the model, e.g. when the loader is given another file.
    Q_INVOKABLE void detach();

    // Makes `name` the one clip that plays, looping from its first frame,
    // and stops the others' animations. Disabling a Timeline puts every
    // property it animated back to its pre-animation value, and enabling
    // one captures the current values as *its* pre-animation values - so
    // the old clip is always disabled before the new one is enabled,
    // otherwise the new clip would capture (and later restore) the old
    // clip's mid-animation pose as the rest pose. That reset is also what
    // makes clips that hide/show props by scale (this model parks unused
    // props at scale 1e-5) not leak props into the next clip. Returns
    // false if there is no clip called `name`.
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
    Q_INVOKABLE bool play(const QString &name, int fadeMs = 0);

signals:
    void clipNamesChanged();
    void currentClipChanged();
    void pausedChanged();

private slots:
    void onFadeFrame();

private:
    // Makes each clip loop seamlessly. RuntimeLoader's Timeline runs
    // 0..length on an infinite loop, but copies the glTF keyframe times
    // as-is - and a glTF clip's first keyframe usually isn't at t=0
    // (CesiumMan.glb: 48 keys at 1/24s..2s, so the first one lands on
    // frame 41.67 of 2000). For the frames before a group's first
    // keyframe, QQuickKeyframeGroup::evaluate() interpolates from the
    // property's *pre-animation* value (the node's rest/bind transform),
    // not from the end of the previous loop. Net effect: at every
    // wrap-around each bone snaps to its rest pose and blends back into
    // the walk cycle over ~42ms - a visible twitch every 2s, i.e. the
    // clip never plays as a closed loop the way other glTF viewers show
    // it.
    //
    // So every keyframe group lacking a keyframe at the timeline's start
    // frame is given one there, carrying the group's *last* keyframe
    // value. The wrap then becomes last pose -> (same pose at frame 0) ->
    // first keyframe, continuous in both position and time, and the loop
    // period stays the clip's own length. Only the gap at the start is
    // filled; a clip whose first and last poses differ still jumps.
    // Returns the number of keyframe groups patched.
    int closeLoops();

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
    // (our Qt fork), which still has the original key times. Returns the
    // number of keys inserted.
    int restoreHolds();

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

    QPointer<QObject> m_sceneRoot;
    QStringList m_clipNames;
    QString m_currentClip;
    bool m_paused = false;

    QPointer<QObject> m_fadeTimeline;
    QList<Faded> m_faded;
    int m_fadeMs = 0;
};
