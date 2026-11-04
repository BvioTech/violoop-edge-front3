import QtQuick

// edge-front2's lib/pages/test/memory_info_page.dart - DDR/storage totals
// from GET /status/system, subjective verdict.
TestPageFrame {
    testId: "memory"
    title: "内存信息"
    description: "核对 DDR/存储容量，目测判定。"

    property var info: ({})

    function gb(bytes) {
        if (bytes === undefined || bytes === null)
            return "-"
        return (bytes / 1e9).toFixed(1) + " GB"
    }

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/status/system")
                return
            info = JSON.parse(body)
            testVerdictStore.setData("memory", {
                ddrTotalBytes: info.ddr ? info.ddr.totalBytes : undefined,
                storageSizeBytes: info.storage ? info.storage.sizeBytes : undefined,
                storageDevice: info.storage ? info.storage.device : undefined,
            })
        }
    }
    Component.onCompleted: restClient.get("/status/system")

    Column {
        spacing: 8
        Text { color: "white"; font.pixelSize: 30; text: "DDR: " + gb(info.ddr ? info.ddr.totalBytes : undefined) }
        Text { color: "white"; font.pixelSize: 30; text: "存储: " + gb(info.storage ? info.storage.sizeBytes : undefined) }
        Text { color: "white"; font.pixelSize: 30; text: "存储设备: " + (info.storage ? info.storage.device : "-") }
    }
}
