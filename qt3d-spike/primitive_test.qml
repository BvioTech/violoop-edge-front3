// Baseline Quick3D sanity scene: a rotating primitive Cube with the
// default material, no glTF/skinning involved. Used to isolate whether
// a render failure is specific to skinned-mesh shaders (see
// runtime_loader.qml + docs/findings.md) or a general Quick3D/EGL
// problem. Run with qml_runner (see its build comment).
import QtQuick
import QtQuick3D

Window {
    // Physical panel is portrait 1080x1240 (see
    // docs/display-stack-overview.md §4); weston is fixed to
    // transform=normal, so the 90° rotation to landscape has to happen
    // here, app-side — same convention as edge-front2's
    // RotatedBox(quarterTurns: 3) (270° clockwise == 90° counter-clockwise).
    width: 1080
    height: 1240
    visible: true
    color: "#202020"

    Item {
        id: rotatedRoot
        width: parent.height  // 1240 logical width after rotation
        height: parent.width  // 1080 logical height after rotation
        anchors.centerIn: parent
        rotation: -90  // counter-clockwise

        View3D {
            id: view3d
            anchors.fill: parent
            environment: SceneEnvironment {
                clearColor: "#303030"
                backgroundMode: SceneEnvironment.Color
            }
            // Centered on the origin, looking straight down -Z — no y
            // offset, so the cube at (0,0,0) lands in the middle of the
            // view instead of being pushed toward an edge.
            PerspectiveCamera { position: Qt.vector3d(0, 0, 400) }
            DirectionalLight { eulerRotation: Qt.vector3d(-45, 45, 0) }
            Model {
                source: "#Cube"
                scale: Qt.vector3d(2, 2, 2)
                materials: [DefaultMaterial { diffuseColor: "orange" }]
                NumberAnimation on eulerRotation.y {
                    from: 0; to: 360; duration: 4000; loops: Animation.Infinite
                }
            }
        }

        Text {
            color: "white"
            font.pixelSize: 28
            text: "fps: " + view3d.renderStats.fps.toFixed(1)
        }
        Timer {
            interval: 2000
            running: true
            repeat: true
            onTriggered: console.log("fps:", view3d.renderStats.fps.toFixed(1))
        }
    }
}
