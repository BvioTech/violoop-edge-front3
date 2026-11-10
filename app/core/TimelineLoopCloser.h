#pragma once

#include <QObject>

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
};
