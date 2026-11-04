import QtQuick

// edge-front2's lib/pages/test/device_info_page.dart - CPU/MCU/board/panel
// info from GET /status/system, operator reads it and judges subjectively
// (no objective pass/fail rule here, same as upstream).
TestPageFrame {
    testId: "device"
    title: "设备信息"
    description: "核对以下硬件/版本信息，目测判定。"

    property var info: ({})

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/status/system")
                return
            info = JSON.parse(body)
            testVerdictStore.setData("device", {
                cpuModel: info.cpu ? info.cpu.model : undefined,
                mcuModel: info.mcu ? info.mcu.model : undefined,
                boardProfile: info.boardProfile,
                boardProfileSource: info.boardProfileSource,
                panelModel: info.panel ? info.panel.model : undefined,
                region: info.region,
                regionSource: info.regionSource,
            })
        }
    }
    Component.onCompleted: restClient.get("/status/system")

    Column {
        spacing: 8
        Text { color: "white"; font.pixelSize: 30; text: "CPU 型号: " + (info.cpu ? info.cpu.model : "-") }
        Text { color: "white"; font.pixelSize: 30; text: "MCU 型号: " + (info.mcu ? info.mcu.model : "-") }
        Text { color: "white"; font.pixelSize: 30; text: "主板档位: " + (info.boardProfile ?? "-") + " (" + (info.boardProfileSource ?? "-") + ")" }
        Text { color: "white"; font.pixelSize: 30; text: "面板型号: " + (info.panel ? info.panel.model : "-") }
        Text { color: "white"; font.pixelSize: 30; text: "区域: " + (info.region ?? "-") + " (" + (info.regionSource ?? "-") + ")" }
        Text { color: "white"; font.pixelSize: 30; text: "SN: " + (info.sn ?? "-") }
    }
}
