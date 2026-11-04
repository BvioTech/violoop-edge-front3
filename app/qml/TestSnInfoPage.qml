import QtQuick

// edge-front2's lib/pages/test/sn_info_page.dart - device SN display,
// subjective verdict.
TestPageFrame {
    testId: "sn"
    title: "序列号"
    description: "核对设备序列号，目测判定。"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/status/system")
                return
            const info = JSON.parse(body)
            snText.text = "SN: " + (info.sn ?? "-")
        }
    }
    Component.onCompleted: restClient.get("/status/system")

    Text {
        id: snText
        color: "white"
        font.pixelSize: 42
        text: "SN: -"
    }
}
