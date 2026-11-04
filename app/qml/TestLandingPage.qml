import QtQuick
import QtQuick.Controls
import "TestDefinitions.js" as TestDefs

// Equivalent of edge-front2's lib/pages/test/test_page.dart: a scrollable
// list of nav cards, each tinted by its TestVerdictStore verdict. Also the
// entry point to the /stress placeholder (kept out of DevMenuPage, since
// upstream's dev menu lists it separately too).
Item {
    id: root

    readonly property var visibleItems: TestDefs.items.filter(function (it) {
        return !it.llmOnly || testVerdictStore.verdicts["llmModel"] !== undefined
    })

    Rectangle {
        id: header
        width: parent.width
        // Auto-sized from content, not a fixed guess: at the app-wide
        // 54px button font (see AppShell.qml) three buttons plus a label
        // wrap onto two lines (Flow below), so a fixed height would clip.
        height: headerCol.implicitHeight + 24
        color: "#181818"

        Column {
            id: headerCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 8

            Flow {
                width: parent.width
                spacing: 12
                Button { text: "< 返回"; onClicked: nav.pop() }
                Text {
                    text: "工厂测试"
                    color: "white"
                    font.pixelSize: 24
                    // No anchors.verticalCenter here - Flow doesn't
                    // support anchors on its children at all ("Flow will
                    // not function" warning, caught offline before this
                    // reached the kiosk). Minor top-alignment vs the
                    // buttons is an acceptable look for this non-1:1 page.
                    topPadding: 12
                }
                Button { text: "重置全部"; onClicked: testVerdictStore.reset() }
                Button { text: "压力测试（占位）"; onClicked: nav.push("TestStressPlaceholderPage.qml") }
            }

            Text {
                color: "#9e9e9e"
                font.pixelSize: 15
                text: {
                    const s = testVerdictStore.summary
                    return "通过 " + s.passed + " / 不通过 " + s.failed + " / 待测 " + s.pending + " / 共 " + s.total
                }
            }
        }
    }

    ListView {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        clip: true
        spacing: 6
        model: root.visibleItems

        delegate: Rectangle {
            width: ListView.view.width
            height: 52
            color: "#1b1b1b"
            border.color: "#333333"
            radius: 4

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16
                StatusDot {
                    anchors.verticalCenter: parent.verticalCenter
                    state: {
                        const v = testVerdictStore.verdicts[modelData.id]
                        return v === "passed" ? "ok" : v === "failed" ? "error" : "pending"
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: "white"
                    font.pixelSize: 18
                    text: modelData.title
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: nav.push(modelData.page)
            }
        }
    }
}
