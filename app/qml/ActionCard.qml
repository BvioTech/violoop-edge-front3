import QtQuick

// Reusable debug-page card chrome: title + free-form content, no
// pass/fail footer (debug cards are action/result, not verdict-based -
// see ../pages/debug/DebugPage.qml). Mirrors the "~10 independent cards,
// each its own mini state machine" shape the Flutter debug_page.dart uses.
Rectangle {
    id: root
    property string title: ""
    // See TestPageFrame.qml's equivalent comment - aliased to .data so
    // inline Connections/Timer helpers work, not just visual Items.
    default property alias contentChildren: contentCol.data

    color: "#1b1b1b"
    border.color: "#333333"
    radius: 8
    implicitHeight: contentCol.implicitHeight + 32

    Column {
        id: contentCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 8

        Text { text: root.title; color: "white"; font.pixelSize: 20; font.bold: true }
    }
}
