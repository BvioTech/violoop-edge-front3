import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/power_test_page.dart - POST
// /power/factory-test/check, read-only PMIC rail health probe (see
// ../../device/server/transports/power_factory_test_handler.js). `ok`
// applied automatically, raw JSON shown for diagnostics.
TestPageFrame {
    testId: "power"
    title: "电源测试"
    description: "检查 PMIC 各路供电电压是否在正常范围内。"

    property string resultText: "(未运行)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/power/factory-test/check")
                return
            resultText = body
            const obj = JSON.parse(body)
            testVerdictStore.setData("power", obj)
            testVerdictStore.setVerdict("power", obj.ok ? "passed" : "failed")
        }
        function onRequestFailed(path, errorString) {
            if (path === "/power/factory-test/check")
                resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "运行电源检查"
            onClicked: {
                resultText = "运行中..."
                restClient.postJson("/power/factory-test/check", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: 600; text: resultText }
    }
}
