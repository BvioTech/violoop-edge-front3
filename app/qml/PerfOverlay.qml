import QtQuick

// What rendering costs, live: frames actually put on screen over the last
// second (counted here), and the machine's CPU load, GPU load, SoC
// temperature and this process's memory (core/SystemStats.h, which only
// reads them while an overlay is up). "-" for a value this machine
// doesn't have. The frame rate drops to 0 when nothing on screen moves:
// nothing is redrawn then.
//
// Once a second the same numbers are handed to perfRecorder
// (core/PerfRecorder.h), which writes them to a CSV file while a
// recording is on and does nothing otherwise. Recording is started and
// stopped from outside the app (SIGUSR2 or EDGE_KIOSK_RECORD_PERF, see
// that header), not from here. The file's name is shown under the
// readout.
Text {
    id: overlay

    // What only the page showing the model knows, for the recording: the
    // animation clip playing and whether it is paused.
    property string clipName: ""
    property bool paused: false

    property int framesSwapped: 0
    property real fps: -1

    horizontalAlignment: Text.AlignRight
    color: "#9e9e9e"
    font.pixelSize: 24

    function show(value, digits, unit) {
        return value < 0 ? "-" : value.toFixed(digits) + unit
    }
    text: "帧率 " + show(fps, 0, " fps")
          + "\nCPU " + show(systemStats.cpuLoad, 0, "%")
          + "\nGPU " + show(systemStats.gpuLoad, 0, "%")
          + "\nSoC " + show(systemStats.socTemp, 1, "°C")
          + "\n内存 " + show(systemStats.rssKb / 1024, 0, " MB")
          + "\n" + (perfRecorder.file ? "记录中 " + perfRecorder.file.split("/").pop() : "未记录")

    Connections {
        target: overlay.Window.window
        function onFrameSwapped() { overlay.framesSwapped += 1 }
    }
    Timer {
        interval: 1000
        repeat: true
        running: true
        property double last: Date.now()
        onTriggered: {
            const now = Date.now()
            overlay.fps = overlay.framesSwapped * 1000 / Math.max(1, now - last)
            overlay.framesSwapped = 0
            last = now
            perfRecorder.record(overlay.fps, overlay.clipName, overlay.paused)
        }
    }
    Binding { target: systemStats; property: "active"; value: true }
    Component.onDestruction: systemStats.active = false
}
