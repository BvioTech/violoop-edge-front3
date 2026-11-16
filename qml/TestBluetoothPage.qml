import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/bluetooth_test_page.dart - POST
// /bluetooth/factory-test/scan (shape confirmed against
// ../../device/server/transports/bluetooth_factory_test_handler.js:
// {ok,elapsedMs,count,scanMs,transport,devices}). `ok` means "saw at
// least one device" - applied automatically, same pattern as wifi, but
// there's no fixed reference device here so the operator still eyeballs
// the list (matches upstream: "subjective, no fixed reference device").
TestPageFrame {
    testId: "bluetooth"
    title: "蓝牙测试"
    description: "触发蓝牙扫描，检查附近设备列表是否合理。"

    property var devices: []
    property string summaryText: "(未运行)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/bluetooth/factory-test/scan")
                return
            const obj = JSON.parse(body)
            devices = obj.devices || []
            summaryText = "count=" + obj.count + " transport=" + obj.transport
            testVerdictStore.setData("bluetooth", obj)
            testVerdictStore.setVerdict("bluetooth", obj.ok ? "passed" : "failed")
        }
        function onRequestFailed(path, errorString) {
            if (path === "/bluetooth/factory-test/scan")
                summaryText = "失败: " + errorString
        }
    }

    Column {
        spacing: 12
        Row {
            spacing: 12
            Button {
                text: "扫描 (8 秒)"
                onClicked: {
                    summaryText = "扫描中..."
                    restClient.postJson("/bluetooth/factory-test/scan", "{}")
                }
            }
            Text { color: "white"; font.pixelSize: 26; anchors.verticalCenter: parent.verticalCenter; text: summaryText }
        }
        Repeater {
            model: devices
            delegate: Text {
                color: "#cccccc"
                font.pixelSize: 24
                text: modelData.name + " (" + modelData.address + ") rssi=" + modelData.rssi
            }
        }
    }
}
