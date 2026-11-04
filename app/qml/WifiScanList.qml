import QtQuick

// Renders a wifi scan result list, agnostic to which REST call populated
// it - test/WifiTestPage.qml uses `POST /wifi/factory-test/scan`
// (judged against reference SSID), debug/DebugWifiPage.qml uses the
// general `GET /wifi/scan` (tap-to-fill into a connect form). Both just
// set `items` to the parsed JSON array and handle `itemClicked`.
Column {
    id: root
    property var items: []
    signal itemClicked(var item)
    spacing: 4

    Repeater {
        model: root.items
        delegate: Rectangle {
            width: root.width
            height: 44
            color: "#202020"
            border.color: "#3a3a3a"

            Row {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 16
                Text {
                    text: modelData.ssid !== undefined ? modelData.ssid : "(hidden)"
                    color: "white"
                    font.pixelSize: 16
                    width: 220
                    elide: Text.ElideRight
                }
                Text {
                    text: modelData.signal !== undefined ? (modelData.signal + "%")
                        : modelData.rssi !== undefined ? (modelData.rssi + " dBm") : ""
                    color: "#9e9e9e"
                    font.pixelSize: 14
                }
                Text {
                    text: modelData.security !== undefined ? modelData.security : ""
                    color: "#9e9e9e"
                    font.pixelSize: 14
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.itemClicked(modelData)
            }
        }
    }
}
