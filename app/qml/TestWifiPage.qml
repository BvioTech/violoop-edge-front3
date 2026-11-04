import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/wifi_test_page.dart - POST
// /wifi/factory-test/scan (shape confirmed against
// ../../device/server/transports/wifi_factory_test_handler.js:
// {ok,elapsedMs,count,total,threshold,v1}). `ok` already encodes "at
// least one AP cleared the factory signal threshold" - the backend does
// the judging, this page just surfaces it (and the reference AP `V1`'s
// own reading, when seen, for antenna-placement diagnostics).
TestPageFrame {
    testId: "wifi"
    title: "WiFi 测试"
    description: "触发 WiFi 扫描，确认能看到足够强度的信号源。"

    property string resultText: "(未运行)"

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/wifi/factory-test/scan")
                return
            const obj = JSON.parse(body)
            resultText = "count=" + obj.count + "/" + obj.total + " threshold=" + obj.threshold
                + (obj.v1 ? ("\nV1: signal=" + obj.v1.signal + "% rssi=" + obj.v1.rssi + "dBm") : "\n(未扫描到参考 AP V1)")
            testVerdictStore.setData("wifi", obj)
            testVerdictStore.setVerdict("wifi", obj.ok ? "passed" : "failed")
        }
        function onRequestFailed(path, errorString) {
            if (path === "/wifi/factory-test/scan")
                resultText = "失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "扫描"
            onClicked: {
                resultText = "扫描中（可能需要几十秒）..."
                restClient.postJson("/wifi/factory-test/scan", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 26; wrapMode: Text.WordWrap; width: 500; text: resultText }
    }
}
