import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/light_test_page.dart - RGB LED on/off
// check via POST /light/factory-test/{solid,off,self-test} (shapes
// confirmed against
// ../../device/server/transports/light_factory_test_handler.js).
TestPageFrame {
    testId: "light"
    title: "灯效测试"
    description: "逐个点亮红/绿/蓝，检查 LED 灯效是否正常，再关闭/自检确认。"

    function solid(r, g, b) {
        restClient.postJson("/light/factory-test/solid", JSON.stringify({r: r, g: g, b: b}))
    }

    Flow {
        width: parent.width
        spacing: 12
        Button { text: "红"; onClicked: solid(255, 0, 0) }
        Button { text: "绿"; onClicked: solid(0, 255, 0) }
        Button { text: "蓝"; onClicked: solid(0, 0, 255) }
        Button { text: "关闭"; onClicked: restClient.postJson("/light/factory-test/off") }
        Button { text: "自检 (R→G→B→W)"; onClicked: restClient.postJson("/light/factory-test/self-test", "{}") }
    }
}
