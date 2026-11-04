import QtQuick
import QtQuick.Controls

// Equivalent of edge-front2's lib/pages/debug/touch/debug_touch_page.dart -
// raw multi-touch diagnostics (position, pressure, contact ellipse) rather
// than TestTouchPage.qml's simple pass/fail trace. No REST calls.
Item {
    id: root

    property int downCount: 0
    property int upCount: 0
    property int cancelCount: 0
    property var log: []

    function appendLog(line) {
        log = [line].concat(log).slice(0, 100)
    }

    Rectangle {
        id: header
        width: parent.width
        height: 140 // bumped again for the larger content font too (see AppShell.qml)
        color: "#181818"
        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6
            Row {
                spacing: 16
                Button { text: "< 返回"; onClicked: nav.pop() }
                Text { text: "触摸诊断"; color: "white"; font.pixelSize: 38; anchors.verticalCenter: parent.verticalCenter }
            }
            Text {
                color: "#9e9e9e"
                font.pixelSize: 24
                text: "down=" + root.downCount + " up=" + root.upCount + " cancel=" + root.cancelCount
            }
        }
    }

    Rectangle {
        id: pad
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: logArea.top
        color: "#141414"
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
            onPressed: function (points) {
                root.downCount += points.length
                for (const p of points)
                    root.appendLog("down id=" + p.pointId + " @(" + p.x.toFixed(0) + "," + p.y.toFixed(0) + ") pressure=" + p.pressure.toFixed(2))
            }
            onReleased: function (points) {
                root.upCount += points.length
                for (const p of points)
                    root.appendLog("up id=" + p.pointId)
            }
            onCanceled: function (points) {
                root.cancelCount += points.length
                root.appendLog("cancel: " + points.length + " point(s)")
            }
        }

        Repeater {
            model: touchArea.touchPoints
            delegate: Item {
                visible: modelData.pressed
                x: modelData.x
                y: modelData.y
                Rectangle {
                    anchors.centerIn: parent
                    width: Math.max(20, modelData.ellipseDiameters.width)
                    height: Math.max(20, modelData.ellipseDiameters.height)
                    radius: width / 2
                    color: "#4caf50"
                    opacity: 0.5
                }
                Text {
                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: 20
                    text: "#" + modelData.pointId + "\np=" + modelData.pressure.toFixed(2)
                }
            }
        }
    }

    ListView {
        id: logArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 180
        clip: true
        model: root.log
        delegate: Text { color: "#888888"; font.pixelSize: 20; text: modelData }
    }
}
