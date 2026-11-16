import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/fan_test_page.dart - buttons drive PWM
// 0%/100% via POST /thermal/factory-test/fan (shape confirmed against
// ../../device/server/transports/thermal_factory_test_handler.js).
TestPageFrame {
    testId: "fan"
    title: "风扇测试"
    description: "驱动风扇 100% 转速，确认能听到/感觉到风扇转动，再归零确认停转。"

    property string resultText: "(未运行)"

    function run(pwm) {
        resultText = "运行中..."
        restClient.postJson("/thermal/factory-test/fan", JSON.stringify({pwm: pwm, durationMs: 3000}))
    }

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/thermal/factory-test/fan")
                return
            resultText = body
        }
        function onRequestFailed(path, errorString) {
            if (path !== "/thermal/factory-test/fan")
                return
            resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Row {
            spacing: 12
            Button { text: "全速 (100%)"; onClicked: run(100) }
            Button { text: "停止 (0%)"; onClicked: run(0) }
        }
        Text { color: "white"; font.pixelSize: 26; wrapMode: Text.WordWrap; width: 500; text: resultText }
    }
}
