// Standalone window for validation item A in isolation - see
// RotationTouchArea.qml for what's actually being checked. Window size and
// the rotation convention (90° app-side, weston stays transform=normal)
// match ../qt3d-spike/primitive_test.qml.
import QtQuick

Window {
    width: 1080
    height: 1240
    visible: true
    color: "#202020"
    title: "ui-interface-spike: rotation + touch"

    Item {
        width: parent.height
        height: parent.width
        anchors.centerIn: parent
        rotation: -90

        RotationTouchArea {
            anchors.fill: parent
        }
    }
}
