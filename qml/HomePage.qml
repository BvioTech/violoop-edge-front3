import QtQuick
import QtQuick.Controls
import EdgeFrontQt.Core

// Home page: the mascot, a rigged 3D model animated live (Qt Quick3D -
// see ../README.md's licensing note: GPL-3 because of it), with the way
// into the developer pages in its top right corner. The status icons
// above it are AppShell.qml's.
//
// To show that this is rendered live rather than played back from a
// video, the viewer can drag to orbit the camera around the character,
// pinch to zoom, and pause the animation and keep orbiting the frozen
// pose.
//
// The model, its lights and its camera are MascotScene.qml; the readout
// in the corner is PerfOverlay.qml. What is here is the gestures, the
// return to the default view, and the controls for trying the model out.
//
// Which clip plays is not decided here but by `mascot` (the app's
// MascotDirector, src/MascotDirector.h): this page tells it when the
// mascot is on screen, passes its directives on to the scene's
// ClipPlayer, and feeds it what only the page knows - taps on the mascot
// and the buttons for picking a clip by hand.
//
// AppShell.qml keeps this page alive for as long as the app runs and
// only hides it under the other pages, so that coming back does not mean
// loading the model again. While hidden nothing is drawn, and the page
// takes the mascot off stage: the clips stop and the director stops
// timing them.
Item {
    id: root

    // Whether a clip picked by hand is overriding the director's choice,
    // and which one was picked last.
    property bool manual: false
    property string manualClip: ""

    // Picks the next (or previous) clip by hand; it loops until another
    // is picked or `manual` is switched off. Counted from the clip picked
    // last, not from the one playing: a pick made during a touch reaction
    // only takes effect once that is over.
    function stepClip(delta) {
        const names = scene.clips.clipNames
        if (names.length === 0)
            return
        const i = names.indexOf(manual ? manualClip : scene.clips.currentClip)
        manualClip = names[(i + delta + names.length) % names.length]
        manual = true
        mascot.set(Mascot.Manual, manualClip)
    }

    onManualChanged: {
        if (!manual)
            mascot.clear(Mascot.Manual)
    }

    Connections {
        target: mascot
        function onDirective(clip, loop, fadeMs) { scene.clips.play(clip, loop, fadeMs) }
    }

    // The director times the clips itself, so it has to know when there
    // is something on screen to time, and when that is frozen. On stage:
    // the model is loaded and this page is the one showing.
    readonly property bool onStage: visible && scene.loadState === "ready"
    property bool welcomed: false
    onOnStageChanged: {
        if (onStage) {
            mascot.stageAttached(scene.clips.clipNames, scene.clips.clipDurations)
            // Once per loaded model, not at every return to this page.
            if (!welcomed) {
                welcomed = true
                mascot.cue(Mascot.Welcome)
            }
        } else {
            mascot.stageDetached()
            if (scene.loadState !== "ready")
                welcomed = false
        }
    }
    // Hidden, the clips stop - also when the page is hidden before the
    // model ever got on stage (the tap on "开发" during the load).
    onVisibleChanged: {
        if (!visible)
            scene.clips.stop()
    }
    Connections {
        target: scene.clips
        function onPausedChanged() { mascot.stagePaused(scene.clips.paused) }
        // A model that finishes loading while this page is hidden starts
        // its first clip all the same (ClipPlayer::attach).
        function onClipNamesChanged() {
            if (!root.visible)
                scene.clips.stop()
        }
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

    MascotScene {
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

        // A tap (a touch that does not turn into a drag) pokes the mascot:
        // on its left, its right, or in the middle. Position in the
        // View3D's own coordinates, for the reason given above.
        //
        // The buttons lie over the scene, and a tap on one of them can
        // reach this handler as well (it only watches, it does not take
        // the touch away from anybody). Those are not pokes.
        TapHandler {
            onTapped: function(point) {
                const over = root.childAt(point.position.x, point.position.y)
                if (over !== scene && !(over instanceof Text))
                    return
                const x = point.position.x
                mascot.cue(x < scene.width / 3 ? Mascot.TouchLeft
                           : x > scene.width * 2 / 3 ? Mascot.TouchRight : Mascot.TouchMiddle)
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

    // Into the developer pages (factory tests, debugging, versions). A
    // plain visible button: edge-front2 hides this behind five taps on
    // its robot picture, but there is no design for this app yet.
    Button {
        id: devButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        text: "开发"
        onClicked: nav.push("DevMenuPage.qml")
    }

    Text {
        id: hint
        anchors.top: devButton.bottom
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
        clipName: scene.clips.currentClip
        paused: scene.clips.paused
    }

    // Loading / failure notice, see MascotScene's loadState. Text only: the
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
        text: scene.clips.paused ? "播放" : "暂停"
        onClicked: scene.clips.paused = !scene.clips.paused
    }

    // Steps through the model's clips by hand, so every one of them can
    // be checked on the device; "自动" hands the choice back to the
    // director.
    Row {
        visible: scene.loadState === "ready" && scene.clips.clipNames.length > 1
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
            text: scene.clips.currentClip + "\n" + (scene.clips.clipNames.indexOf(scene.clips.currentClip) + 1) + " / " + scene.clips.clipNames.length
        }
        Button {
            text: "▶"
            onClicked: root.stepClip(1)
        }
        Button {
            visible: root.manual
            text: "自动"
            onClicked: root.manual = false
        }
    }
}
