import QtQuick
import QtQuick.Controls

// Placeholder for edge-front2's lib/pages/test/stress_page.dart - a
// continuous 2s-polling soak/aging state machine (POST
// /system/factory-test/stress/{start,status,stop} and
// /aging/{start,status,stop}), deliberately not implemented this pass
// (user-confirmed scope cut - see docs/progress.md). Kept as its own
// reachable page (not just missing) so it's visibly "not done yet" rather
// than silently absent from navigation.
Item {
    id: root

    Column {
        anchors.centerIn: parent
        spacing: 16
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#999999"
            font.pixelSize: 32
            text: "压力测试 / 老化测试 — 占位，未实现"
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#666666"
            font.pixelSize: 24
            text: "对应 edge-front2 的 /stress 页面，本轮范围未覆盖"
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "< 返回"
            onClicked: nav.pop()
        }
    }
}
