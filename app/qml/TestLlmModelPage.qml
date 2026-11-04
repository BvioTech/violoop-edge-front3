import QtQuick

// edge-front2's lib/pages/test/llm_model_test_page.dart - VLA11-only
// (gated in TestDefinitions.js/TestVerdictStore via boardProfile),
// periodic GET /health against the separate violoop-edge-llm service on
// :8000 (not proxied by violoop-edge - see CLAUDE.md's note in
// edge-front2 and main.cpp's `llmRestClient`). Real shape confirmed on
// dev1: {available_models,default_model,loaded_model,models,queue_depth,
// status}.
TestPageFrame {
    testId: "llmModel"
    title: "LLM 模型测试"
    description: "确认本地 LLM 推理服务已加载模型（仅 VLA11）。"

    property var info: ({})

    Connections {
        target: llmRestClient
        function onRequestSucceeded(path, statusCode, body) {
            if (path !== "/health")
                return
            info = JSON.parse(body)
            testVerdictStore.setData("llmModel", info)
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: llmRestClient.get("/health")
    }

    Column {
        spacing: 8
        Text { color: "white"; font.pixelSize: 17; text: "状态: " + (info.status ?? "-") }
        Text { color: "white"; font.pixelSize: 17; text: "已加载模型: " + (info.loaded_model ?? "-") }
        Text { color: "white"; font.pixelSize: 17; text: "队列深度: " + (info.queue_depth !== undefined ? info.queue_depth : "-") }
    }
}
