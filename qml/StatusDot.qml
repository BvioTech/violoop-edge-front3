import QtQuick

// Small colored status indicator reused across the status header, the
// factory-test landing list and the debug cards - equivalent in spirit to
// edge-front2's StatusDot widget (green/red/neutral verdict at a glance).
Rectangle {
    id: root
    // "ok" | "warn" | "error" | "neutral" | "pending"
    property string state: "neutral"
    width: 14
    height: 14
    radius: width / 2
    color: {
        switch (state) {
        case "ok": return "#4caf50"
        case "warn": return "#ffb300"
        case "error": return "#e53935"
        case "pending": return "#9e9e9e"
        default: return "#555555"
        }
    }
    border.color: Qt.darker(color, 1.3)
    border.width: 1
}
