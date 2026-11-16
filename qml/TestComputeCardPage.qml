import QtQuick

// edge-front2's lib/pages/test/compute_card_test_page.dart - periodic
// GET /status/accelerator (real shape confirmed on dev1: {present,model,
// health,temperatureC,deviceStatus,firmwareVersion,...}), subjective
// verdict (operator judges health/temp).
TestPageFrame {
    testId: "computeCard"
    title: "算力卡测试"
    description: "确认算力卡在线、健康状态正常、温度合理。"

    property var info: ({})

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/status/accelerator")
                return
            info = JSON.parse(body)
            testVerdictStore.setData("computeCard", info)
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: restClient.get("/status/accelerator")
    }

    Column {
        spacing: 8
        Text { color: "white"; font.pixelSize: 30; text: "型号: " + (info.model ?? "-") }
        Text { color: "white"; font.pixelSize: 30; text: "状态: " + (info.deviceStatus ?? "-") + " / 健康: " + (info.health ?? "-") }
        Text { color: "white"; font.pixelSize: 30; text: "温度: " + (info.temperatureC !== undefined ? info.temperatureC + "°C" : "-") }
        Text { color: "white"; font.pixelSize: 30; text: "固件: " + (info.firmwareVersion ?? "-") + " / 软件: " + (info.softwareVersion ?? "-") }
    }
}
