import QtQuick

// Validation item A (docs/findings.md): the one risk flagged in both
// ../qt3d-spike/README.md ("已知限制": "触摸坐标对齐完全没测") and
// ../../edge-front2/docs/display-stack-overview.md §8.3 that the 3D spike
// never touched. edge-front2's own lib/shared/widgets/edge_swipe.dart
// found that Flutter's RotatedBox correctly rotates hit-test *positions*
// but NOT GestureDetector's reported drag *velocity* vector - hence that
// widget hand-rolls velocity from raw Listener pointer events instead of
// trusting the high-level recognizer. This component exists to find out
// whether Qt's TapHandler/DragHandler have the same split (position
// rotated, velocity not) when embedded inside a rotated Item, or not -
// there is no a-priori answer, it has to be read off a real touchscreen.
//
// Expected to be embedded inside the same `rotation: -90` Item convention
// ../qt3d-spike/primitive_test.qml established (physical panel is portrait
// 1080x1240; this component itself is written in already-rotated logical
// coordinates, same as everything else inside that Item).
Item {
    id: root

    property string lastTapCorner: "(none)"
    property string lastTapInfo: "(none)"
    property string lastDragInfo: "(none)"

    function logTap(corner, point) {
        lastTapCorner = corner
        lastTapInfo = corner + " @ (" + point.position.x.toFixed(0) + ", " + point.position.y.toFixed(0) + ")"
        console.log("rotation_touch: tap", lastTapInfo)
    }

    // Corner markers: 80x80 squares in each logical corner. A tap landing
    // on the wrong visual corner (as reported by lastTapInfo vs where the
    // finger actually touched the panel) would mean hit-testing itself
    // isn't honoring the rotation - checked first, before velocity, since
    // if this fails nothing downstream is trustworthy either.
    Repeater {
        model: [
            { name: "topLeft", x: 0, y: 0 },
            { name: "topRight", x: root.width - 80, y: 0 },
            { name: "bottomLeft", x: 0, y: root.height - 80 },
            { name: "bottomRight", x: root.width - 80, y: root.height - 80 },
        ]
        delegate: Rectangle {
            x: modelData.x
            y: modelData.y
            width: 80
            height: 80
            color: root.lastTapCorner === modelData.name ? "lime" : "#555"
            border.color: "white"
            TapHandler {
                onTapped: (eventPoint) => root.logTap(modelData.name, eventPoint)
            }
        }
    }

    // Central drag area: reports translation + velocity as DragHandler
    // sees them. A real swipe (e.g. left-to-right across the rotated
    // logical width) should report a translation/velocity vector pointing
    // the same way a person watching the screen would call "left to
    // right" - if it instead points along the *physical* panel's
    // unrotated axis, that is exactly the edge_swipe.dart-style bug this
    // component is here to catch.
    DragHandler {
        id: drag
        target: null // report-only, don't actually move anything
        onActiveChanged: {
            if (!active) {
                root.lastDragInfo = "translation=(" + translation.x.toFixed(0) + ", " + translation.y.toFixed(0) +
                    ") velocity=(" + centroid.velocity.x.toFixed(0) + ", " + centroid.velocity.y.toFixed(0) + ")"
                console.log("rotation_touch: drag ended", root.lastDragInfo)
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 8
        Text { color: "white"; font.pixelSize: 24; text: "tap a corner, then swipe anywhere" }
        Text { color: "yellow"; font.pixelSize: 22; text: "last tap: " + root.lastTapInfo }
        Text { color: "yellow"; font.pixelSize: 22; text: "last drag: " + root.lastDragInfo }
    }
}
