import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/mcu_ping_test_page.dart - single-shot
// objective test: POST /sign/factory-test/ping -> {ok,total,passed,mcu,
// results} (shape confirmed against
// ../../device/server/transports/sign_factory_test_handler.js). The
// endpoint's own `ok` field is the pass/fail verdict - auto-applied, same
// as Flutter's setXResult(passed:...) pattern for objective tests, while
// the manual 通过/不通过 footer buttons (from TestPageFrame) stay
// available for the operator to override.
TestPageFrame {
    testId: "mcuPing"
    title: "MCU 通信测试"
    description: "测试与 MCU 的串口握手（PING）。"

    property string resultText: "(未运行)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/sign/factory-test/ping")
                return
            resultText = body
            const obj = JSON.parse(body)
            testVerdictStore.setVerdict("mcuPing", obj.ok ? "passed" : "failed")
        }
        function onRequestFailed(path, errorString) {
            if (path === "/sign/factory-test/ping")
                resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "运行 PING 测试"
            onClicked: {
                resultText = "运行中..."
                restClient.postJson("/sign/factory-test/ping", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 14; wrapMode: Text.WordWrap; width: 500; text: resultText }
    }
}
