// Standalone window for validation item D in isolation - see
// VoiceStateView.qml for what's actually being checked.
import QtQuick

Window {
    width: 1080
    height: 1240
    visible: true
    color: "#202020"
    title: "ui-interface-spike: voice state"

    Item {
        width: parent.height
        height: parent.width
        anchors.centerIn: parent
        rotation: -90

        VoiceStateView {
            anchors.fill: parent
        }
    }

    Component.onCompleted: {
        if (typeof sseClient !== "undefined")
            sseClient.start()
    }
}
