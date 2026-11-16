.pragma library

// Canonical factory-test id/title/page list, order matching
// TestVerdictStore.cpp's kIds (which itself mirrors edge-front2's
// lib/stores/test_store.dart `sections` getter) - the id strings here
// MUST match TestVerdictStore exactly, they're the key the C++ side uses
// to record verdicts and build the POST /factory-test/data snapshot.
// `llmOnly` sections only render once TestVerdictStore has added them
// (see setLlmModelVisible(), gated on boardProfile === 'VLA11').
//
// `usbInput` deliberately has no entry here - edge-front2 implements that
// page but excludes it from the menu/summary (its own comment: "已停用：
// 不进入汇总"); this app matches that by routing to
// TestUsbInputPage.qml only via a direct nav.push() call, never listed
// here.
var items = [
    { id: "device", title: "设备信息", page: "TestDeviceInfoPage.qml" },
    { id: "memory", title: "内存信息", page: "TestMemoryInfoPage.qml" },
    { id: "sn", title: "序列号", page: "TestSnInfoPage.qml" },
    { id: "display", title: "显示", page: "TestDisplayPage.qml" },
    { id: "touch", title: "触摸", page: "TestTouchPage.qml" },
    { id: "backlight", title: "背光", page: "TestBacklightPage.qml" },
    { id: "fan", title: "风扇", page: "TestFanPage.qml" },
    { id: "light", title: "灯效", page: "TestLightPage.qml" },
    { id: "audio", title: "音频", page: "TestAudioPage.qml" },
    { id: "audioEcho", title: "录放回声", page: "TestAudioEchoPage.qml" },
    { id: "mcuPing", title: "MCU 通信", page: "TestMcuPingPage.qml" },
    { id: "button", title: "物理按键", page: "TestButtonPage.qml" },
    { id: "wifi", title: "WiFi", page: "TestWifiPage.qml" },
    { id: "bluetooth", title: "蓝牙", page: "TestBluetoothPage.qml" },
    { id: "power", title: "电源", page: "TestPowerPage.qml" },
    { id: "usb", title: "USB-A", page: "TestUsbPage.qml" },
    { id: "hdmi", title: "HDMI", page: "TestHdmiPage.qml" },
    { id: "computeCard", title: "算力卡", page: "TestComputeCardPage.qml" },
    { id: "llmModel", title: "LLM 模型", page: "TestLlmModelPage.qml", llmOnly: true },
]
