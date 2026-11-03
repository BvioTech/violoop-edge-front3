import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils

Window {
    width: 1240
    height: 1080
    visible: true
    color: "#202020"
    title: "qt3d-spike: RuntimeLoader"

    // Pass the glTF/glb path as argv[1] (see scripts/launch-under-weston.sh).
    property string assetPath: Qt.application.arguments.length > 1
        ? Qt.application.arguments[1]
        : "assets/CesiumMan.glb"

    View3D {
        id: view3d
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#303030"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        PerspectiveCamera {
            position: Qt.vector3d(0, 1.2, 3.5)
            eulerRotation: Qt.vector3d(-10, 0, 0)
            // Default clipNear (10) is larger than this camera's distance to
            // the model (~3.5, meter-scale glTF asset), so the whole model
            // was being clipped away. Use clip planes that fit meter-scale content.
            clipNear: 0.1
            clipFar: 100
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-45, 45, 0)
            brightness: 1.2
        }

        RuntimeLoader {
            source: Qt.resolvedUrl(assetPath)
            onStatusChanged: console.log("RuntimeLoader status:", status, "errorString:", errorString)
            onBoundsChanged: console.log("RuntimeLoader bounds: min=", bounds.minimum, "max=", bounds.maximum)
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 12
        color: "white"
        font.pixelSize: 28
        text: "fps: " + view3d.renderStats.fps.toFixed(1) +
              "  sync(ms): " + view3d.renderStats.syncTime.toFixed(2) +
              "  render(ms): " + view3d.renderStats.renderTime.toFixed(2)
    }

    // Periodic performance sample: fps/sync/render come from Quick3D here;
    // CPU/memory/GPU/temperature are sampled in C++ (perfMonitor, see
    // perf_monitor.cpp) since QML has no access to procfs/sysfs. Logged to
    // CSV ($QT3D_SPIKE_PERF_LOG, default $TMPDIR/qt3d_spike_perf.csv) and
    // to stdout - see docs/findings.md for how to read the results.
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            // perfMonitor is only registered by qt3d_spike's main.cpp, not
            // by qml_runner.cpp - guard so this file still loads under both.
            if (typeof perfMonitor !== "undefined")
                perfMonitor.logSample(
                    view3d.renderStats.fps,
                    view3d.renderStats.syncTime,
                    view3d.renderStats.renderTime)
        }
    }
}
