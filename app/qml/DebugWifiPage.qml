import QtQuick
import QtQuick.Controls

// Equivalent of edge-front2's lib/pages/debug/wifi/wifi_config_page.dart -
// general-purpose scan (POST /wifi/scan -> {ok,count,networks}) + connect
// (POST /wifi/connect {ssid,password?} -> {ok,ssid,ip}), shapes confirmed
// against ../../device/server/transports/wifi_config_handler.js. Distinct
// from TestWifiPage.qml's factory-test scan (different endpoint/purpose).
Item {
    id: root

    property var networks: []
    property string scanStatus: ""
    property string ssidField: ""
    property string passwordField: ""
    property string connectStatus: ""

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path === "/wifi/scan") {
                const obj = JSON.parse(body)
                networks = obj.networks || []
                scanStatus = "找到 " + (obj.count ?? networks.length) + " 个网络"
            } else if (path === "/wifi/connect") {
                const obj = JSON.parse(body)
                connectStatus = obj.ok ? ("已连接: " + obj.ssid + " ip=" + obj.ip) : ("失败: " + body)
            }
        }
        function onRequestFailed(path, errorString) {
            if (path === "/wifi/scan") scanStatus = "扫描失败: " + errorString
            else if (path === "/wifi/connect") connectStatus = "连接失败: " + errorString
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Row {
            spacing: 16
            Button { text: "< 返回"; onClicked: nav.pop() }
            Text { text: "WiFi 配置"; color: "white"; font.pixelSize: 42; anchors.verticalCenter: parent.verticalCenter }
        }

        Row {
            spacing: 12
            Button {
                text: "扫描"
                onClicked: {
                    scanStatus = "扫描中..."
                    restClient.postJson("/wifi/scan", "{}")
                }
            }
            Text { color: "#cccccc"; font.pixelSize: 24; anchors.verticalCenter: parent.verticalCenter; text: scanStatus }
        }

        WifiScanList {
            width: parent.width
            items: root.networks
            onItemClicked: (item) => { root.ssidField = item.ssid; ssidInput.text = item.ssid }
        }

        Row {
            spacing: 12
            TextField { id: ssidInput; width: 220; placeholderText: "SSID"; onTextChanged: root.ssidField = text }
            TextField { id: pwInput; width: 220; placeholderText: "密码"; echoMode: TextInput.Password; onTextChanged: root.passwordField = text }
            Button {
                text: "连接"
                onClicked: {
                    connectStatus = "连接中..."
                    restClient.postJson("/wifi/connect", JSON.stringify({ssid: root.ssidField, password: root.passwordField}))
                }
            }
        }
        Text { color: "#cccccc"; font.pixelSize: 24; text: connectStatus }
    }
}
