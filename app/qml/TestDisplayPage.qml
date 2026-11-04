import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/display_test_page.dart - full-screen color
// sweep, operator eyeballs, no REST call.
TestPageFrame {
    testId: "display"
    title: "显示测试"
    description: "依次点击色块，检查屏幕显示是否正常（无色偏/亮点/暗点）。"

    Row {
        spacing: 12
        Repeater {
            model: [
                { name: "红", color: "#ff0000" },
                { name: "绿", color: "#00ff00" },
                { name: "蓝", color: "#0000ff" },
                { name: "白", color: "#ffffff" },
                { name: "黑", color: "#000000" },
            ]
            delegate: Button {
                text: modelData.name
                onClicked: sweep.color = modelData.color
            }
        }
        Button { text: "退出全屏色块"; onClicked: sweep.visible = false }
    }

    Rectangle {
        id: sweep
        anchors.fill: parent
        visible: false
        color: "#ff0000"
        MouseArea { anchors.fill: parent; onClicked: sweep.visible = false }
    }
}
