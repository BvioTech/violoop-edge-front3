// Standalone window for validation item E in isolation - see
// CjkTextSample.qml for what's actually being checked.
import QtQuick

Window {
    width: 1080
    height: 1240
    visible: true
    color: "#202020"
    title: "ui-interface-spike: CJK text"

    Item {
        width: parent.height
        height: parent.width
        anchors.centerIn: parent
        rotation: -90

        CjkTextSample {
            anchors.centerIn: parent
        }
    }
}
