import QtQuick

// edge-front2's lib/pages/test/hdmi_test_page.dart - reuses the same
// StatusStore polling the home status header already runs (GET
// /status/hdmi every 5s), no separate request here.
TestPageFrame {
    testId: "hdmi"
    title: "HDMI 测试"
    description: "确认 HDMI 输入已连接并识别到合理的分辨率。"

    Column {
        spacing: 8
        Text {
            color: "white"
            font.pixelSize: 30
            text: "状态: " + (!statusStore.hdmiKnown ? "读取中..." : statusStore.hdmiConnected ? "已连接" : "未连接")
        }
        Repeater {
            model: statusStore.hdmiKnown ? statusStore.hdmiInputs : []
            delegate: Text {
                color: "#cccccc"
                font.pixelSize: 26
                text: modelData.label + (modelData.primary ? " (主)" : "") + ": " + (modelData.connected ? "已连接" : "未连接")
            }
        }
    }
}
