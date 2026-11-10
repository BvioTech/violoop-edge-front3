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

    View3D {
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#121212"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            position: Qt.vector3d(0, 1.2, 3.5)
            eulerRotation: Qt.vector3d(-10, 0, 0)
            // Default clipNear (10) clips away meter-scale glTF content at
            // this camera distance (~3.5) - same fix runtime_loader.qml
            // needed, see that file's comment.
            clipNear: 0.1
            clipFar: 100
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-45, 45, 0)
            brightness: 1.2
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
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 16
        color: "#9e9e9e"
        font.pixelSize: 32
        text: "待机"
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
