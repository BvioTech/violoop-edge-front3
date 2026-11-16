import QtQuick
import QtQuick.Controls

// Away/standby screen: continuously renders a glTF skeletal (skinned)
// animation. Unlike edge-front2's AwayPage (lib/pages/away/away_page.dart),
// which composites a pre-baked MP4 over a poster image, this renders the
// animation live - that's the whole reason this module links Qt Quick3D
// (see ../../README.md's licensing note: GPL-3 because of it) instead of
// playing a video file.
//
// The point of this page is to show the animation is rendered live from a
// rigged 3D model rather than played back from a video, so the viewer can
// drag to orbit the camera around the character, pinch to zoom, and pause
// the animation mid-stride and keep orbiting the frozen pose.
//
// The model, its lights and its camera are AwayScene.qml; the readout in
// the corner is PerfOverlay.qml. What is here is the gestures, the return
// to the default view, and the controls for trying the model out.
Item {
    id: root

    function stepClip(delta) {
        const names = scene.clipNames
        if (names.length === 0)
            return
        const i = names.indexOf(scene.currentClip)
        scene.playClip(names[(i + delta + names.length) % names.length])
    }

    // It's a standby screen: don't leave it parked at whatever odd angle
    // the last person dragged it to. A few seconds after the last touch,
    // ease back to the default view.
    function interactionStarted() {
        returnTimer.stop()
        returnAnimation.stop()
    }
    function interactionEnded() {
        returnTimer.restart()
    }

    Timer {
        id: returnTimer
        interval: 4000
        onTriggered: {
            // yaw accumulates freely across full turns while dragging;
            // return to the nearest equivalent of the default rather than
            // unwinding every turn.
            yawReturn.to = scene.defaultYaw + Math.round((scene.yaw - scene.defaultYaw) / 360) * 360
            returnAnimation.start()
        }
    }

    ParallelAnimation {
        id: returnAnimation
        NumberAnimation { id: yawReturn; target: scene; property: "yaw"; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: scene; property: "pitch"; to: scene.defaultPitch; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: scene; property: "distance"; to: scene.defaultDistance; duration: 900; easing.type: Easing.InOutCubic }
    }

    AwayScene {
        id: scene
        anchors.fill: parent

        // One finger: orbit. target: null - the handler only reports the
        // drag, it must not move the View3D itself.
        //
        // Deltas come from centroid.position (in the View3D's own
        // coordinates), NOT activeTranslation: the latter is in scene
        // (window) coordinates, and AppShell.qml rotates the whole UI -90
        // degrees for the physical panel, so on the device a scene-space
        // drag is a quarter turn off from what the finger does on the
        // picture (found on dev1: swiping sideways pitched the camera).
        DragHandler {
            target: null
            property point last: Qt.point(0, 0)
            onActiveChanged: {
                last = centroid.position
                if (active)
                    root.interactionStarted()
                else
                    root.interactionEnded()
            }
            onCentroidChanged: {
                if (!active)
                    return
                // Dragging right/down pulls the character's surface along
                // with the finger, i.e. the camera orbits the other way.
                scene.yaw -= (centroid.position.x - last.x) * 0.4
                scene.pitch = Math.max(-60, Math.min(20, scene.pitch - (centroid.position.y - last.y) * 0.4))
                last = centroid.position
            }
        }

        // Two fingers: zoom (camera distance). Rotation/translation of the
        // pinch are ignored.
        PinchHandler {
            target: null
            property real startDistance: scene.defaultDistance
            onActiveChanged: {
                if (active) {
                    startDistance = scene.distance
                    root.interactionStarted()
                } else {
                    root.interactionEnded()
                }
            }
            onActiveScaleChanged: {
                if (active)
                    scene.distance = Math.max(scene.minDistance, Math.min(scene.maxDistance, startDistance / activeScale))
            }
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 16
        color: "#9e9e9e"
        font.pixelSize: 32
        text: "待机"
    }

    Text {
        id: hint
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        color: "#616161"
        font.pixelSize: 24
        text: "滑动旋转 · 双指缩放"
    }

    PerfOverlay {
        anchors.top: hint.bottom
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 16
        clipName: scene.currentClip
        paused: scene.paused
    }

    // Loading / failure notice, see AwayScene's loadState. Text only: the
    // GUI thread is stuck inside the load for most of the wait, so
    // anything animated here would just sit frozen and look hung. (A
    // render-thread Animator does keep moving, but on dev1 it made the
    // model appear a full second later - 3.0s instead of 1.9s from page
    // creation.)
    Text {
        visible: scene.loadState !== "ready"
        anchors.centerIn: parent
        color: "#9e9e9e"
        font.pixelSize: 32
        text: scene.loadState === "error" ? "模型加载失败" : "模型加载中…"
    }

    // Freezes the animation. Orbiting still works while paused.
    Button {
        visible: scene.loadState === "ready"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 16
        text: scene.paused ? "播放" : "暂停"
        onClicked: scene.setPaused(!scene.paused)
    }

    // Steps through the model's clips, so every one of them can be checked
    // on the device. Stand-in for the real thing (events picking clips).
    Row {
        visible: scene.loadState === "ready" && scene.clipNames.length > 1
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 16
        spacing: 12

        Button {
            text: "◀"
            onClicked: root.stepClip(-1)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 330
            horizontalAlignment: Text.AlignHCenter
            color: "#9e9e9e"
            font.pixelSize: 28
            text: scene.currentClip + "\n" + (scene.clipNames.indexOf(scene.currentClip) + 1) + " / " + scene.clipNames.length
        }
        Button {
            text: "▶"
            onClicked: root.stepClip(1)
        }
    }

    // No hidden-gesture/idle-timeout entry or exit yet (matches this app's
    // existing explicit-button navigation everywhere else, see
    // HomePage.qml's comment on skipping edge-front2's 5-tap gesture) -
    // this page is reachable from DevMenuPage.qml for now.
    Button {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 16
        text: "< 返回"
        onClicked: nav.pop()
    }
}
