import QtQuick
import QtQuick.Controls

// Equivalent of edge-front2's lib/pages/debug/debug_page.dart: a
// scrollable stack of independent action/status cards. Endpoints
// confirmed against ../../device/server/transports/local_http.js's doc
// comments; exact response shapes not dug into for the lower-stakes cards
// here the way wifi/bluetooth/power/upgrade were (see app/docs/progress.md) -
// each just calls the right endpoint with a reasonable body and shows the
// raw response, which is the actual "business alignment" bar for
// dev-only tooling cards like these.
Item {
    id: root

    property string authorizeResult: ""
    property real volumePercent: 0
    property string volumeResult: ""
    property string pushResult: ""
    property string replayResult: ""
    property string resourceResult: ""
    property string sshResult: ""
    property string factoryResetState: "idle" // idle | confirm | armed | failed | waiting

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path === "/sign/request-authorization") authorizeResult = body
            else if (path === "/audio/volume" && statusCode !== undefined) {
                try {
                    const obj = JSON.parse(body)
                    if (obj.percent !== undefined) volumePercent = obj.percent
                } catch (e) { /* POST ack without a percent field */ }
                volumeResult = body
            }
            else if (path === "/debug/push") pushResult = body
            else if (path === "/upgrade/debug/progress") replayResult = body
            else if (path === "/audio/factory-test/install" || path === "/audio/factory-test/cleanup") resourceResult = body
            else if (path === "/ssh/enable" || path === "/ssh/disable") sshResult = body
            else if (path === "/system/factory-reset") factoryResetState = "waiting"
        }
        function onRequestFailed(path, errorString) {
            if (path === "/system/factory-reset") factoryResetState = "failed"
        }
    }

    Connections {
        target: sseClient
        function onEventReceived(type, data) {
            if (type === "factory-reset:result")
                factoryResetState = "armed"
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: col.implicitHeight
        clip: true

        Column {
            id: col
            width: parent.width
            spacing: 12

            Row {
                spacing: 16
                Button { text: "< 返回"; onClicked: nav.pop() }
                Text { text: "调试"; color: "white"; font.pixelSize: 26; anchors.verticalCenter: parent.verticalCenter }
            }

            ActionCard {
                width: parent.width
                title: "设备状态"
                Text {
                    color: "#cccccc"
                    font.pixelSize: 14
                    text: "wifi=" + statusStore.wifiConnected + " hdmi=" + statusStore.hdmiConnected
                        + " usbHost=" + statusStore.usbHostConnected + " pairingSerial=" + statusStore.pairingSerialOpen
                }
                Button {
                    text: "跳转: 触摸诊断"
                    onClicked: nav.push("DebugTouchPage.qml")
                }
                Button {
                    text: "跳转: WiFi 配置"
                    onClicked: nav.push("DebugWifiPage.qml")
                }
            }

            ActionCard {
                width: parent.width
                title: "请求授权"
                Button {
                    text: "发起授权请求"
                    onClicked: {
                        root.authorizeResult = "请求中..."
                        restClient.postJson("/sign/request-authorization", "{}")
                    }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: parent.width; text: root.authorizeResult }
            }

            ActionCard {
                width: parent.width
                title: "音量"
                Row {
                    spacing: 12
                    Slider {
                        width: 300
                        from: 0; to: 100
                        value: root.volumePercent
                        onMoved: restClient.postJson("/audio/volume", JSON.stringify({percent: Math.round(value)}))
                    }
                    Button { text: "读取当前音量"; onClicked: restClient.get("/audio/volume") }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; text: root.volumeResult }
            }

            ActionCard {
                width: parent.width
                title: "调试推送 (debug/push)"
                Button {
                    text: "发送测试推送"
                    onClicked: {
                        root.pushResult = "发送中..."
                        restClient.postJson("/debug/push", JSON.stringify({title: "调试", body: "来自 Qt 调试页的测试推送"}))
                    }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: parent.width; text: root.pushResult }
            }

            ActionCard {
                width: parent.width
                title: "升级进度回放 (upgrade/debug/progress)"
                Row {
                    spacing: 12
                    Button {
                        text: "开始回放"
                        onClicked: {
                            root.replayResult = "回放中..."
                            restClient.postJson("/upgrade/debug/progress?scenario=run", "{}")
                        }
                    }
                    Button {
                        text: "取消回放"
                        onClicked: restClient.postJson("/upgrade/debug/progress?scenario=cancel", "{}")
                    }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; wrapMode: Text.WordWrap; width: parent.width; text: root.replayResult }
            }

            ActionCard {
                width: parent.width
                title: "音频测试资源包"
                Row {
                    spacing: 12
                    Button {
                        text: "安装"
                        onClicked: restClient.postJson("/audio/factory-test/install", "{}")
                    }
                    Button {
                        text: "清理"
                        onClicked: restClient.postJson("/audio/factory-test/cleanup", "{}")
                    }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; text: root.resourceResult }
            }

            ActionCard {
                width: parent.width
                title: "SSH"
                Row {
                    spacing: 12
                    Button { text: "开启"; onClicked: restClient.postJson("/ssh/enable", "{}") }
                    Button { text: "关闭"; onClicked: restClient.postJson("/ssh/disable", "{}") }
                }
                Text { color: "#cccccc"; font.pixelSize: 13; text: root.sshResult }
            }

            ActionCard {
                width: parent.width
                title: "恢复出厂设置"
                Text {
                    color: "#e57373"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    width: parent.width
                    text: "注意: armed/rebootRequired 之后绝不能热重启（面板会黑屏锁死），必须物理断电重启。"
                }
                Button {
                    text: factoryResetState === "idle" ? "恢复出厂设置" : "状态: " + factoryResetState
                    enabled: factoryResetState === "idle" || factoryResetState === "confirm"
                    onClicked: {
                        if (factoryResetState !== "confirm") {
                            factoryResetState = "confirm"
                            return
                        }
                        restClient.postJson("/system/factory-reset", "{}")
                    }
                }
                Text {
                    visible: factoryResetState === "confirm"
                    color: "#ffb300"
                    font.pixelSize: 13
                    text: "再次点击确认执行恢复出厂设置"
                }
            }
        }
    }
}
