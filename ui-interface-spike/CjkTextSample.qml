import QtQuick

// Validation item E (docs/findings.md): edge-front2 hit real tofu-box
// regressions from Flutter/Skia's text stack not doing per-glyph font
// fallback correctly on this device, and works around it with an explicit
// fallback chain (../../edge-front2/lib/theme/app_typography.dart:
// ['DejaVu Sans', 'Noto Sans Kannada', 'Noto Sans CJK SC', 'Noto Color
// Emoji']). Qt's font matching/fallback machinery (Fontconfig-based on
// Linux) is a different code path from Skia's - this component exists to
// find out empirically whether mixed CJK/Latin/emoji text in one Text
// element renders correctly with Qt's *default* fallback (no explicit
// font.family set below, deliberately - that's the risk case), not to
// pre-guess the answer.
Column {
    spacing: 16

    Text { color: "white"; font.pixelSize: 32; text: "Pure Latin: The quick brown fox" }
    Text { color: "white"; font.pixelSize: 32; text: "纯中文：设备正在检查蓝牙和麦克风" }
    Text { color: "white"; font.pixelSize: 32; text: "Mixed 混排 123: WiFi 已连接，温度 47.2°C" }
    Text { color: "white"; font.pixelSize: 32; text: "Emoji 混排: 任务完成 ✅ 🔔 设备温度告警 ⚠️" }

    // Same mixed line again but with an explicit CJK family set, as a
    // side-by-side comparison for the screenshot - if the default-fallback
    // line above shows tofu/missing glyphs but this one doesn't, that
    // confirms it's a fallback-chain problem (fixable the same way
    // edge-front2 fixed it) rather than the font simply not being
    // installed on the device at all.
    Text {
        color: "white"
        font.pixelSize: 32
        font.family: "Noto Sans CJK SC"
        text: "显式指定字体: Mixed 混排 123 ✅"
    }
}
