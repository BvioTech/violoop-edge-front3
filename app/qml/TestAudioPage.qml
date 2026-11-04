import QtQuick
import QtQuick.Controls

// edge-front2's lib/pages/test/audio_test_page.dart - pure trigger/status
// UI, no local audio library (confirmed: playback happens device-side via
// the backend's own `aplay`, see
// ../../edge-front2/lib/services/audio_service.dart and
// ../../device/server/transports - this page is the same REST-trigger
// pattern as the other factory-test action pages, nothing audio-specific
// on the Qt side).
TestPageFrame {
    testId: "audio"
    title: "音频播放测试"
    description: "选择一段测试音频循环播放，操作员听音判断，完成后停止。"

    property var files: []
    property string resultText: ""

    Connections {
        target: restClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path === "/audio/factory-test/files") {
                // {ok,dir,count,files:[{name,path,bytes,builtin}]}
                files = JSON.parse(body).files || []
            } else if (path.indexOf("/audio/factory-test/") === 0) {
                resultText = body
            }
        }
        function onRequestFailed(path, errorString) {
            if (path.indexOf("/audio/factory-test/") === 0)
                resultText = "失败: " + errorString
        }
    }
    Component.onCompleted: restClient.get("/audio/factory-test/files")

    Column {
        width: parent.width
        spacing: 12
        Flow {
            width: parent.width
            spacing: 12
            Button { text: "刷新列表"; onClicked: restClient.get("/audio/factory-test/files") }
            Button {
                text: "安装测试音频包"
                onClicked: restClient.postJson("/audio/factory-test/install", "{}")
            }
            Button {
                text: "停止播放"
                onClicked: restClient.postJson("/audio/factory-test/stop", "{}")
            }
        }
        Repeater {
            model: files
            delegate: Button {
                text: "循环播放: " + modelData.name
                onClicked: restClient.postJson(
                    "/audio/factory-test/play-file",
                    JSON.stringify({path: modelData.path, loop: true}))
            }
        }
        Text { color: "#bbbbbb"; font.pixelSize: 14; wrapMode: Text.WordWrap; width: 500; text: resultText }
    }
}
