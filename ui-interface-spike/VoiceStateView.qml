import QtQuick

// Validation item D (docs/findings.md): a simplified stand-in for
// edge-front2's lib/pages/voice/widgets/voice_horizon.dart, which the UI
// survey flagged as the single highest-complexity widget in the app (a
// hand-rolled CustomPainter spectrum driven by SSE realtime:state/:vad
// events, not a stock animation). This component only needs to answer two
// narrower questions, not reproduce the visual: can Qt Canvas do smooth
// per-frame custom painting driven by live SSE data without stalling the
// UI thread, and does that combination leak memory over time (checked
// separately in combined.qml / validation F).
//
// Reduced to 4 states (connecting/listening/speaking/failed) from the real
// widget's 7 (../../edge-front2/lib/pages/voice/voice_page.dart has
// connecting/listening/thinking/working/speaking/reconnecting/failed) -
// enough to prove state-driven re-rendering works, not a product spec.
Item {
    id: root

    property string state: "connecting"
    property real level: 0.4 // 0..1, driven by realtime:vad when available
    property bool liveDataSeen: false
    property int frameCount: 0

    // Falls back to cycling states on its own if no real realtime:state
    // event shows up (e.g. no mic session was started against the real
    // device) - the painting/animation path still needs to be exercisable
    // standalone, per the plan's "没有真实事件时用一个 Timer 模拟状态轮转
    // 做兜底".
    Timer {
        id: fallbackCycle
        interval: 3000
        running: !root.liveDataSeen
        repeat: true
        property var states: ["connecting", "listening", "speaking", "failed"]
        property int idx: 0
        onTriggered: {
            idx = (idx + 1) % states.length
            root.state = states[idx]
        }
    }

    // Wired up here (rather than requiring the embedding scene to do it)
    // so both the standalone voice_state.qml window and combined.qml get
    // the same real-SSE-if-available behavior for free.
    Connections {
        target: typeof sseClient !== "undefined" ? sseClient : null
        function onEventReceived(type, data) {
            if (type !== "realtime:state" && type !== "realtime:vad")
                return
            root.liveDataSeen = true
            try {
                const payload = JSON.parse(data)
                if (type === "realtime:state" && payload.state)
                    root.state = payload.state
                if (type === "realtime:vad" && typeof payload.level === "number")
                    root.level = Math.max(0, Math.min(1, payload.level))
            } catch (e) {
                console.log("voice_state: failed to parse SSE payload:", e, data)
            }
        }
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Threaded

        property real phase: 0

        FrameAnimation {
            running: true
            onTriggered: {
                canvas.phase += 0.12
                canvas.requestPaint()
                root.frameCount++
            }
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = "#202020"
            ctx.fillRect(0, 0, width, height)

            const barCount = 24
            const barWidth = width / barCount * 0.6
            const gap = width / barCount
            const midY = height / 2

            let color = "#6cf"
            if (root.state === "listening") color = "#6cf"
            else if (root.state === "speaking") color = "#9f6"
            else if (root.state === "failed") color = "#f66"
            else color = "#888" // connecting / resting / anything else

            ctx.fillStyle = color
            for (let i = 0; i < barCount; i++) {
                let amp
                if (root.state === "failed") {
                    amp = 6 // flat line, no activity
                } else if (root.state === "connecting") {
                    amp = 10 + 6 * Math.sin(canvas.phase + i * 0.3)
                } else {
                    // listening/speaking: level-scaled pseudo-spectrum, each
                    // bar offset so it reads as motion rather than a static
                    // shape - this is the per-frame repaint stress case.
                    const base = root.state === "speaking" ? 0.6 : 0.3
                    amp = (10 + 50 * (base + root.level) * Math.abs(Math.sin(canvas.phase * 1.5 + i * 0.5)))
                }
                const x = i * gap + (gap - barWidth) / 2
                ctx.fillRect(x, midY - amp / 2, barWidth, amp)
            }
        }
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            if (typeof perfMonitor !== "undefined")
                perfMonitor.logSample(root.frameCount, 0, 0)
            root.frameCount = 0
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        color: "white"
        font.pixelSize: 20
        text: "voice_state: " + root.state + (root.liveDataSeen ? " (live SSE)" : " (fallback timer)")
    }
}
