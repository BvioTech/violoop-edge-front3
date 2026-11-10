import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.AssetUtils

// Away/standby screen: continuously renders a glTF skeletal (skinned)
// animation via Quick3D's RuntimeLoader, the approach qt3d-spike spent
// several real-hardware rounds validating (see
// ../../qt3d-spike/docs/findings.md and ../../qt3d-spike/runtime_loader.qml,
// which this page's View3D/camera/light setup mirrors). Unlike edge-front2's
// AwayPage (lib/pages/away/away_page.dart), which composites a pre-baked
// MP4 over a poster image, this renders the animation live - that's the
// whole reason this module links Qt Quick3D (see ../../README.md's licensing
// note: GPL-3 because of it) instead of playing a video file.
//
// RuntimeLoader auto-plays whatever animation is baked into the glTF: its
// generated QtQuickTimeline TimelineAnimation defaults to `running: true`
// and `loops: Animation.Infinite` (confirmed by reading
// qtquick3d/src/assetutils/qssgqmlutilities.cpp), so no Timer/loop wiring
// is needed to keep it playing. What it does NOT do is close the loop:
// glTF keyframes rarely start at t=0, and the timeline fills that gap by
// blending in from the rest pose on every wrap-around (a twitch once per
// cycle). timelineLoopCloser (core/TimelineLoopCloser.h) patches the
// generated timeline once the model has loaded - see that header.
//
// Asset: assets/away/CesiumMan.glb, Khronos glTF-Sample-Models' CesiumMan
// (CC0), the same model qt3d-spike's real-hardware testing (including the
// 6-hour soak run) used - a placeholder until a production mascot model
// exists. Swap the RuntimeLoader's `source` for a new asset (also added to
// CMakeLists.txt's RESOURCES and tracked via git-lfs, see
// ../../.gitattributes) when one does.
Item {
    id: root

    // Orbit camera: the point of this page is to show the animation is
    // rendered live from a rigged 3D model rather than played back from a
    // video (what edge-front2 does), so the viewer can drag to orbit the
    // camera around the character, pinch to zoom, and pause the animation
    // mid-stride and keep orbiting the frozen pose. The defaults below
    // reproduce the original fixed camera exactly (position (0, 1.2, 3.5),
    // pitched -10 degrees): that line of sight crosses the character's
    // axis (x = z = 0) at y = 1.2 - 3.5 * tan(10deg) ~= 0.58, which is the
    // orbit pivot, 3.5 / cos(10deg) ~= 3.55 away.
    readonly property real defaultYaw: 0
    readonly property real defaultPitch: -10
    readonly property real defaultDistance: 3.55
    property real yaw: defaultYaw
    property real pitch: defaultPitch
    property real distance: defaultDistance
    property bool paused: false

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
            yawReturn.to = root.defaultYaw + Math.round((root.yaw - root.defaultYaw) / 360) * 360
            returnAnimation.start()
        }
    }

    ParallelAnimation {
        id: returnAnimation
        NumberAnimation { id: yawReturn; target: root; property: "yaw"; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "pitch"; to: root.defaultPitch; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "distance"; to: root.defaultDistance; duration: 900; easing.type: Easing.InOutCubic }
    }

    View3D {
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#121212"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        // Yaw and pitch on separate nested nodes so the orbit doesn't
        // depend on Euler-angle composition order.
        Node {
            position: Qt.vector3d(0, 0.58, 0)
            eulerRotation.y: root.yaw

            Node {
                eulerRotation.x: root.pitch

                PerspectiveCamera {
                    z: root.distance
                    // Default clipNear (10) clips away meter-scale glTF
                    // content at this camera distance (~3.5) - same fix
                    // runtime_loader.qml needed, see that file's comment.
                    clipNear: 0.1
                    clipFar: 100
                }
            }
        }

        // Both lights stay fixed in the world (not parented to the camera
        // rig) on purpose: the shading changing as the camera orbits is
        // part of what shows this is a lit 3D scene.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-45, 45, 0)
            brightness: 1.2
        }

        // Weak fill from roughly the opposite side, so the character's
        // back isn't near-black once the camera orbits behind the key
        // light.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-20, -135, 0)
            brightness: 0.4
        }

        RuntimeLoader {
            id: loader
            source: Qt.resolvedUrl("../assets/away/CesiumMan.glb")
            onStatusChanged: {
                if (status === RuntimeLoader.Success)
                    timelineLoopCloser.closeLoops(loader)
                else if (status === RuntimeLoader.Error)
                    console.log("AwayPage: RuntimeLoader failed to load", source, "-", errorString)
            }
        }

        // One finger: orbit. target: null - the handler only reports the
        // drag, it must not move the View3D itself.
        //
        // Deltas come from centroid.position (in this View3D's own
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
                root.yaw -= (centroid.position.x - last.x) * 0.4
                root.pitch = Math.max(-60, Math.min(20, root.pitch - (centroid.position.y - last.y) * 0.4))
                last = centroid.position
            }
        }

        // Two fingers: zoom (camera distance). Rotation/translation of the
        // pinch are ignored.
        PinchHandler {
            target: null
            property real startDistance: root.defaultDistance
            onActiveChanged: {
                if (active) {
                    startDistance = root.distance
                    root.interactionStarted()
                } else {
                    root.interactionEnded()
                }
            }
            onActiveScaleChanged: {
                if (active)
                    root.distance = Math.max(1.5, Math.min(8, startDistance / activeScale))
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
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        color: "#616161"
        font.pixelSize: 24
        text: "滑动旋转 · 双指缩放"
    }

    // Freezes the generated TimelineAnimation (unreachable from QML, hence
    // the C++ helper - see core/TimelineLoopCloser.h). Orbiting still
    // works while paused.
    Button {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 16
        text: root.paused ? "播放" : "暂停"
        onClicked: {
            root.paused = !root.paused
            timelineLoopCloser.setPaused(loader, root.paused)
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
