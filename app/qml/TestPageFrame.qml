import QtQuick
import QtQuick.Controls

// Shared chrome for factory-test sub-pages (see
// ../pages/test/TestDefinitions.js and ../pages/test/*.qml): title,
// description, a status dot bound to TestVerdictStore, a content slot for
// the page-specific bit, and the "通过/不通过" footer every one of
// edge-front2's 19 test pages has. Business-logic parity, not visual
// parity - most pages just drop a short `content` into this frame instead
// of hand-rolling their own header/footer chrome.
Item {
    id: root

    property string testId: ""
    property string title: ""
    property string description: ""
    // Child items placed inside a TestPageFrame{} instance land here.
    // Aliased to Item.data (not .children): pages routinely also declare
    // non-visual helpers (Connections, Timer) inline, and only `data`
    // accepts arbitrary QObjects the way a plain Item's own default
    // property would.
    default property alias contentChildren: contentArea.data

    readonly property string verdict: testVerdictStore.verdicts[testId] ?? "pending"

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Row {
            width: parent.width
            spacing: 16
            Button { text: "< 返回"; onClicked: Nav.pop() }
            Text {
                text: root.title
                color: "white"
                font.pixelSize: 26
                anchors.verticalCenter: parent.verticalCenter
            }
            StatusDot {
                anchors.verticalCenter: parent.verticalCenter
                state: root.verdict === "passed" ? "ok" : root.verdict === "failed" ? "error" : "pending"
            }
        }

        Text {
            visible: root.description.length > 0
            text: root.description
            color: "#bbbbbb"
            font.pixelSize: 16
            width: parent.width
            wrapMode: Text.WordWrap
        }

        Item {
            id: contentArea
            width: parent.width
            height: parent.height - y - footerRow.height - parent.spacing
        }

        Row {
            id: footerRow
            spacing: 12
            Button {
                text: "通过"
                highlighted: root.verdict === "passed"
                onClicked: testVerdictStore.setVerdict(root.testId, "passed")
            }
            Button {
                text: "不通过"
                highlighted: root.verdict === "failed"
                onClicked: testVerdictStore.setVerdict(root.testId, "failed")
            }
        }
    }
}
