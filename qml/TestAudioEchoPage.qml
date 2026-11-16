import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/audio_echo_test_page.dart - two-step flow:
// POST /audio/factory-test/capture?ms=3000 (backend records mic input,
// streams back WAV bytes + X-Saved/X-Capture-Ms/X-Elapsed-Ms response
// headers - confirmed against
// ../../device/server/transports/audio_factory_test_handler.js), then
// POST /audio/factory-test/play-file with the X-Saved path to play it
// back through the speaker. This needs RestClient's response-header
// extension (see src/RestClient.h).
TestPageFrame {
    testId: "audioEcho"
    title: "录放回声测试"
    description: "录制 3 秒环境声，再回放确认麦克风和扬声器都正常。"

    property string statusText: "(未开始)"
    property string lastSavedPath: ""

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body, rawSize, headers) {
            if (path !== "/audio/factory-test/capture?ms=3000")
                return
            lastSavedPath = headers["X-Saved"] || ""
            statusText = "录音完成: " + rawSize + " 字节, 耗时 " + headers["X-Capture-Ms"] + "ms, 保存于 " + lastSavedPath
            if (lastSavedPath)
                restClient.postJson("/audio/factory-test/play-file", JSON.stringify({path: lastSavedPath}))
        }
        function onRequestFailed(path, errorString) {
            if (path === "/audio/factory-test/capture?ms=3000")
                statusText = "录音失败: " + errorString
        }
    }

    Column {
        spacing: 16
        Button {
            text: "录音 3 秒并回放"
            onClicked: {
                statusText = "录音中..."
                // capture is POST, not GET (confirmed against the backend
                // handler's doc comment).
                restClient.postJson("/audio/factory-test/capture?ms=3000", "{}")
            }
        }
        Text { color: "white"; font.pixelSize: 26; wrapMode: Text.WordWrap; width: 500; text: statusText }
    }
}
