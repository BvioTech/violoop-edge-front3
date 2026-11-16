import QtQuick

// edge-front2's lib/pages/test/touch_test_page.dart - full-screen
// touch-trace overlay, subjective, no REST call. (The detailed raw
// pointer/pressure diagnostics live in DebugTouchPage.qml, matching
// edge-front2's split between this page and debug_touch_page.dart.)
TestPageFrame {
    testId: "touch"
    title: "触摸测试"
    description: "用手指在下方区域滑动/多点触摸，圆点应跟手，检查触摸是否准确。"

    Rectangle {
        anchors.fill: parent
        color: "#1a1a1a"
        border.color: "#333333"

        MultiPointTouchArea {
            id: touchArea
            anchors.fill: parent
            maximumTouchPoints: 10
            touchPoints: [
                TouchPoint { id: p0 }, TouchPoint { id: p1 }, TouchPoint { id: p2 },
                TouchPoint { id: p3 }, TouchPoint { id: p4 }, TouchPoint { id: p5 },
                TouchPoint { id: p6 }, TouchPoint { id: p7 }, TouchPoint { id: p8 },
                TouchPoint { id: p9 }
            ]
        }

        Repeater {
            model: touchArea.touchPoints
            delegate: Rectangle {
                visible: modelData.pressed
                x: modelData.x - width / 2
                y: modelData.y - height / 2
                width: 40
                height: 40
                radius: 20
                color: "#4caf50"
                opacity: 0.8
            }
        }
    }
}
