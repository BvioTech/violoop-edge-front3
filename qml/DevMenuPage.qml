import QtQuick
import QtQuick.Controls

// Equivalent of edge-front2's lib/pages/dev/dev_menu_page.dart entry list
// (only the three routes this pass implements - `/stress` is a separate
// placeholder route reachable from TestLandingPage, not from here, same
// as upstream where it's not in the dev menu either... actually upstream
// DOES list /stress here too; kept out of this menu deliberately since
// this pass only stubs it - see TestStressPlaceholderPage.qml).
Item {
    id: root

    Column {
        anchors.centerIn: parent
        spacing: 16

        Button {
            width: 480
            height: 150
            text: "工厂测试"
            onClicked: nav.push("TestLandingPage.qml")
        }
        Button {
            width: 480
            height: 150
            text: "调试"
            onClicked: nav.push("DebugPage.qml")
        }
        Button {
            width: 480
            height: 150
            text: "版本和升级"
            onClicked: nav.push("VersionPage.qml")
        }
        Button {
            width: 480
            height: 150
            text: "< 返回主页"
            onClicked: nav.pop()
        }
    }
}
