import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/backlight_test_page.dart - slider drives
// GET/POST /system/backlight (real shape confirmed against
// ../../device/server/transports/backlight_handler.js:
// {device,brightness,maxBrightness,actualBrightness,percent}).
TestPageFrame {
    testId: "backlight"
    title: "背光测试"
    description: "拖动滑块调整背光，检查屏幕亮度是否跟随变化。"

    property int currentPercent: 50

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/system/backlight")
                return
            const obj = JSON.parse(body)
            if (obj.percent !== undefined && obj.percent !== null)
                currentPercent = obj.percent
        }
    }
    Component.onCompleted: restClient.get("/system/backlight")

    Column {
        spacing: 16
        Text { color: "white"; font.pixelSize: 17; text: "当前亮度: " + currentPercent + "%" }
        Slider {
            width: 400
            from: 0
            to: 100
            value: currentPercent
            onMoved: restClient.postJson("/system/backlight", JSON.stringify({percent: Math.round(value)}))
        }
    }
}
