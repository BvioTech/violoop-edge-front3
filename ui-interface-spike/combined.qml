// Validation item F (docs/findings.md): everything running together -
// rotated touch area (A), REST polling + SSE stream against the real
// `violoop-edge` service (B/C), and the SSE-driven animated widget (D) -
// for a 15-30 minute real-device soak, to check whether adding network
// I/O + 2D custom painting on top of the already-proven-stable Qt/EGL
// baseline (../qt3d-spike/docs/findings.md's 6-hour static-3D-scene soak)
// introduces any *new* leak/perf regression. Not meant to re-prove
// anything qt3d-spike already settled on its own.
import QtQuick

Window {
    width: 1080
    height: 1240
    visible: true
    color: "#202020"
    title: "ui-interface-spike: combined"

    property string restStatus: "(no REST response yet)"
    property string sseStatus: "connecting..."
    property int heartbeatCount: 0
    property int eventCount: 0

    Timer {
        interval: 5000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            if (typeof restClient !== "undefined")
                restClient.get("/status/service")
        }
    }

    Connections {
        target: typeof restClient !== "undefined" ? restClient : null
        function onRequestSucceeded(path, statusCode, body) {
            restStatus = "GET " + path + " -> " + statusCode + " (" + body.length + " bytes)"
            console.log("combined: REST ok", restStatus)
        }
        function onRequestFailed(path, errorString) {
            restStatus = "GET " + path + " -> FAILED: " + errorString
            console.log("combined: REST failed", restStatus)
        }
    }

    Connections {
        target: typeof sseClient !== "undefined" ? sseClient : null
        function onConnected() {
            sseStatus = "connected"
            console.log("combined: SSE connected")
        }
        function onDisconnected(reason) {
            sseStatus = "disconnected (" + reason + ") - reconnecting"
            console.log("combined: SSE disconnected:", reason)
        }
        function onHeartbeat() {
            heartbeatCount++
            console.log("combined: SSE heartbeat #" + heartbeatCount)
        }
        function onEventReceived(type, data) {
            eventCount++
            console.log("combined: SSE event", type, data)
        }
    }

    Component.onCompleted: {
        if (typeof sseClient !== "undefined")
            sseClient.start()
    }

    Item {
        width: parent.height // 1240 logical width after rotation
        height: parent.width // 1080 logical height after rotation
        anchors.centerIn: parent
        rotation: -90

        Column {
            anchors.fill: parent

            Rectangle {
                width: parent.width
                height: 70
                color: "#111"
                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    spacing: 32
                    Text { color: "white"; font.pixelSize: 18; text: "REST: " + restStatus }
                    Text { color: "white"; font.pixelSize: 18; text: "SSE: " + sseStatus + " | heartbeats=" + heartbeatCount + " events=" + eventCount }
                }
            }

            Row {
                width: parent.width
                height: 420

                VoiceStateView {
                    width: parent.width / 2
                    height: parent.height
                }
                Rectangle {
                    width: parent.width / 2
                    height: parent.height
                    color: "#202020"
                    CjkTextSample {
                        anchors.centerIn: parent
                    }
                }
            }

            RotationTouchArea {
                width: parent.width
                height: parent.height - 70 - 420
            }
        }
    }
}
