import QtQuick
import QtQuick.Controls

// Home page: deliberately NOT 1:1 with edge-front2's home_page.dart (the
// suggestion/task carousel, clock, etc.) - the user explicitly scoped this
// down to "status icons correct" (shown globally in AppShell's
// StatusHeader, not here) plus a working path into the dev menu. The real
// app has a hidden 5-tap gesture on the robot image for that; no design
// exists yet, so this is a plain visible button for now (see
// app/docs/progress.md).
Item {
    id: root

    Column {
        anchors.centerIn: parent
        spacing: 24

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: "#666666"
            font.pixelSize: 32
            text: "主页（占位，无设计稿）"
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "开发菜单"
            font.pixelSize: 54 // ~3x the Basic style's default button text size
            implicitWidth: 480
            implicitHeight: 150
            onClicked: nav.push("DevMenuPage.qml")
        }
    }
}
