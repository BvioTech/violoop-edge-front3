import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/button_test_page.dart - POST
// /sign/factory-test/button sends a 32B payload and waits up to ~10s for a
// physical button press (shape confirmed against
// ../../device/server/transports/sign_factory_test_handler.js: 200 =
// pressed, 408 = timed out). RestClient's 10s transfer timeout matches
// the device's own window, so a real timeout surfaces as a normal
// requestSucceeded with statusCode 408, not a transport error.
TestPageFrame {
    testId: "button"
    title: "物理按键测试"
    description: "点击开始后 10 秒内按下设备上的物理按键。"

    property string resultText: "(未开始)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/sign/factory-test/button")
                return
            if (statusCode === 200) {
                resultText = "按下成功: " + body
                testVerdictStore.setVerdict("button", "passed")
            } else if (statusCode === 408) {
                resultText = "超时未按下: " + body
                testVerdictStore.setVerdict("button", "failed")
            } else {
                resultText = "错误 (" + statusCode + "): " + body
            }
        }
        function onRequestFailed(path, errorString) {
            if (path === "/sign/factory-test/button")
                resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "开始（10 秒内按键）"
            onClicked: {
                resultText = "等待按键..."
                restClient.postJson("/sign/factory-test/button", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 24; wrapMode: Text.WordWrap; width: 500; text: resultText }
    }
}
