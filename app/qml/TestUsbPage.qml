import QtQuick

// edge-front2's lib/pages/test/usb_test_page.dart - USB-A (OTG) port,
// purely subjective: operator plugs a device into the test host and
// judges by hand, no REST call at all (matches upstream exactly).
TestPageFrame {
    testId: "usb"
    title: "USB-A 测试"
    description: "将测试设备插入 USB-A 口，确认主机端能识别（无需本页操作）。"

    Text {
        color: "#bbbbbb"
        font.pixelSize: 28
        text: "纯目测测试，无接口调用。"
    }
}
