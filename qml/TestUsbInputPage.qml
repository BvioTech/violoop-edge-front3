import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/usb_input_test_page.dart - implemented but
// deliberately NOT listed in TestDefinitions.js/TestLandingPage.qml,
// matching upstream ("currently disabled/hidden... reachable only by
// direct URL"). POST /usb-input/factory-test/probe (shape confirmed
// against ../../device/server/transports/usb_input_factory_test_handler.js).
TestPageFrame {
    testId: "usbInput"
    title: "USB 输入设备探测（未在菜单中）"
    description: "探测 USB Type-A 主机口上是否接有键鼠/U盘等设备。"

    property string resultText: "(未运行)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/usb-input/factory-test/probe")
                return
            resultText = body
            const obj = JSON.parse(body)
            testVerdictStore.setVerdict("usbInput", obj.ok ? "passed" : "failed")
        }
        function onRequestFailed(path, errorString) {
            if (path === "/usb-input/factory-test/probe")
                resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "探测"
            onClicked: {
                resultText = "探测中..."
                restClient.postJson("/usb-input/factory-test/probe", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 24; wrapMode: Text.WordWrap; width: 600; text: resultText }
    }
}
