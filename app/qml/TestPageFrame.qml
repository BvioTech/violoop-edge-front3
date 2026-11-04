import QtQuick
import QtQuick.Controls
import "TestDefinitions.js" as TestDefs

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

    // Tapping 通过/不通过 both record the verdict AND jump straight to
    // the next test in TestDefinitions.js's canonical order (operator
    // workflow: mark and move on, not mark-then-navigate-back-to-the-
    // list-then-tap-the-next-one). Falls back to just going back to the
    // landing page at the end of the list, or for a testId not in the
    // list at all (TestUsbInputPage - deliberately excluded from the
    // menu, see TestDefinitions.js) where there's no "next" to speak of.
    // `nav.replace()`, not pop()+push(): those would be two separate
    // currentPageChanged() emissions, which would flash the landing page
    // for one frame in between.
    function goToNext() {
        const items = TestDefs.items.filter(function (it) {
            return !it.llmOnly || testVerdictStore.verdicts["llmModel"] !== undefined
        })
        const idx = items.findIndex(function (it) { return it.id === root.testId })
        if (idx >= 0 && idx + 1 < items.length)
            nav.replace(items[idx + 1].page)
        else
            nav.pop()
    }

    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Row {
            width: parent.width
            spacing: 16
            Button { text: "< 返回"; onClicked: nav.pop() }
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
                onClicked: {
                    testVerdictStore.setVerdict(root.testId, "passed")
                    root.goToNext()
                }
            }
            Button {
                text: "不通过"
                highlighted: root.verdict === "failed"
                onClicked: {
                    testVerdictStore.setVerdict(root.testId, "failed")
                    root.goToNext()
                }
            }
        }
    }
}
