import QtQuick
import QtQuick.Controls
import EdgeFrontQt.Core

// Home page: the mascot, a rigged 3D model animated live (Qt Quick3D -
// see ../README.md's licensing note: GPL-3 because of it), with the way
// into the developer pages in its top right corner. The status icons
// above it are AppShell.qml's.
//
// The model, its lights and its camera are MascotScene.qml; the readout
// in the corner is PerfOverlay.qml. The only gesture is the tap; the view
// is fixed.
//
// Which clip plays is not decided here but by `mascot` (the app's
// MascotDirector, src/MascotDirector.h): this page tells it when the
// mascot is on screen, passes its directives on to the scene's
// ClipPlayer, and feeds it what only the page knows - taps on the mascot.
//
// AppShell.qml keeps this page alive for as long as the app runs and
// only hides it under the other pages, so that coming back does not mean
// loading the model again. While hidden nothing is drawn, and the page
// takes the mascot off stage: the clips stop and the director stops
// timing them.
Item {
    id: root

    Connections {
        target: mascot
        function onDirective(clip, loop, fadeMs) { scene.clips.play(clip, loop, fadeMs) }
    }

    // The director times the clips itself, so it has to know when there
    // is something on screen to time. On stage:
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
        // A model that finishes loading while this page is hidden starts
        // its first clip all the same (ClipPlayer::attach).
        function onClipNamesChanged() {
            if (!root.visible)
                scene.clips.stop()
        }
    }

    MascotScene {
        id: scene
        anchors.fill: parent
        steadyLamp: mascot.steadyLamp

        // A tap pokes the mascot, in one of seven zones that between them
        // cover the whole view (the designer's touch-zones.json): three
        // columns split at 0.34 and 0.66 of the width, the outer two cut
        // in two at 0.38 of the height, the middle one in three at 0.34
        // and 0.68. The position is in the View3D's own coordinates, not in
        // scene (window) coordinates: AppShell.qml rotates the whole UI
        // -90 degrees for the physical panel, so on the device the two
        // are a quarter turn apart.
        //
        // The button lies over the scene, and a tap on it can reach this
        // handler as well (it only watches, it does not take the touch
        // away from anybody). That is not a poke.
        TapHandler {
            onTapped: function(point) {
                const over = root.childAt(point.position.x, point.position.y)
                if (over !== scene && !(over instanceof Text))
                    return
                const x = point.position.x / scene.width
                const y = point.position.y / scene.height
                if (x < 0.34)
                    mascot.cue(y < 0.38 ? Mascot.TouchUpperLeft : Mascot.TouchLeft)
                else if (x >= 0.66)
                    mascot.cue(y < 0.38 ? Mascot.TouchUpperRight : Mascot.TouchRight)
                else
                    mascot.cue(y < 0.34 ? Mascot.TouchUpperMiddle
                               : y >= 0.68 ? Mascot.TouchLowerMiddle : Mascot.TouchMiddle)
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

    PerfOverlay {
        anchors.top: devButton.bottom
        anchors.right: parent.right
        anchors.topMargin: 16
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
}
