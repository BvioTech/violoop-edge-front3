import QtQuick

// Home page's actual requirement ("左上角设备状态图标正确"): wifi,
// cloud/backend reachability, HDMI input(s), USB-HID gadget host link,
// pairing serial - same fields edge-front2's AppStatusHeader `_StatusIcons`
// shows (lib/shared/widgets/app_scaffold.dart), bound to the same
// `/status/*` polling (StatusStore). No icon glyphs/fonts exist yet (no
// design has landed) - dots + short labels stand in for them; swap for
// real icons once a design does, the state logic underneath doesn't change.
Row {
    id: root
    spacing: 24

    Row {
        spacing: 6
        StatusDot {
            anchors.verticalCenter: parent.verticalCenter
            state: !statusStore.wifiKnown ? "neutral" : statusStore.wifiConnected ? "ok" : "error"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: 28
            text: "WiFi" + (statusStore.wifiKnown && statusStore.wifiConnected ? (" " + statusStore.wifiSignal + "%") : "")
        }
    }

    Row {
        spacing: 6
        StatusDot {
            anchors.verticalCenter: parent.verticalCenter
            state: !statusStore.connectivityKnown ? "neutral"
                : !statusStore.connectivityOk ? "error"
                : statusStore.connectivityLatencyMs < 300 ? "ok"
                : statusStore.connectivityLatencyMs < 1000 ? "warn" : "error"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: "white"
            font.pixelSize: 28
            text: "云端" + (statusStore.connectivityKnown && statusStore.connectivityOk ? (" " + statusStore.connectivityLatencyMs + "ms") : "")
        }
    }

    Repeater {
        model: statusStore.hdmiKnown ? statusStore.hdmiInputs : []
        delegate: Row {
            spacing: 6
            StatusDot {
                anchors.verticalCenter: parent.verticalCenter
                state: modelData.connected ? "ok" : "neutral"
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                color: "white"
                font.pixelSize: 28
                text: modelData.label
            }
        }
    }

    Row {
        spacing: 6
        StatusDot {
            anchors.verticalCenter: parent.verticalCenter
            state: !statusStore.hardwareKnown ? "neutral" : statusStore.usbHostConnected ? "ok" : "error"
        }
        Text { anchors.verticalCenter: parent.verticalCenter; color: "white"; font.pixelSize: 28; text: "USB" }
    }

    Row {
        spacing: 6
        StatusDot {
            anchors.verticalCenter: parent.verticalCenter
            state: !statusStore.hardwareKnown ? "neutral" : statusStore.pairingSerialOpen ? "ok" : "error"
        }
        Text { anchors.verticalCenter: parent.verticalCenter; color: "white"; font.pixelSize: 28; text: "配对口" }
    }
}
