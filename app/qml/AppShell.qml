import QtQuick
import QtQuick.Controls

// Root window: physical-panel rotation (same convention as both spikes -
// see ../../qt3d-spike/primitive_test.qml and
// ../../ui-interface-spike/combined.qml, real-machine-confirmed in the
// latter's validation item A), a persistent StatusHeader, and a `Loader`
// driven by `nav.currentPage` (core/NavController.h/.cpp) for page
// navigation.
//
// This is the third navigation mechanism tried - both QML-native ones
// failed in ways invisible until real-device touch testing (see
// NavController.h's comment for the full story): a hand-rolled `pragma
// Singleton` QtObject's own `function push()` threw "not a function" at
// call time despite loading cleanly, and `StackView`'s documented
// `StackView.view` attached property read back `null` from inside the
// pushed item's own handlers, for both `initialItem` and an explicitly
// `push()`-ed item alike. Plain C++ QObject + Q_PROPERTY, the same
// pattern RestClient/StatusStore/etc. already use reliably all through
// this app, sidesteps both failure classes entirely.
Window {
    id: window
    width: 1080
    height: 1240
    visible: true
    color: "#121212"
    title: "violoop-edge-kiosk-qt"

    // StatusStore's `sn`/`boardProfile` land asynchronously after the
    // first /status/system poll - keep TestVerdictStore in sync whenever
    // they change rather than reading them once at startup.
    Connections {
        target: statusStore
        function onSystemChanged() {
            testVerdictStore.setDeviceSn(statusStore.deviceSn)
            testVerdictStore.setLlmModelVisible(statusStore.boardProfile === "VLA11")
        }
    }

    Item {
        id: rotatedRoot
        // Bound to window.width/height directly, NOT parent.width/height
        // (parent here is window.contentItem): under the wayland QPA
        // backend, contentItem's size lags window.width/height by at
        // least one event-loop tick at startup (confirmed via a one-off
        // diagnostic log: window was already 1080x1240 while
        // parent.height/parent.width both still read 0). StackView's
        // initialItem push happens synchronously against whatever size
        // rotatedRoot has at that moment and does not retry once the
        // real size arrives, so the old parent.height/parent.width
        // bindings left it permanently stuck pushing against a 0-sized
        // stack (depth stayed 0, nothing ever rendered). window.width/
        // height are the literal properties set a few lines up, correct
        // from the very first synchronous evaluation.
        width: window.height // 1240 logical width after rotation
        height: window.width // 1080 logical height after rotation
        anchors.centerIn: parent
        rotation: -90

        Column {
            id: col
            anchors.fill: parent

            Rectangle {
                width: parent.width
                height: 64
                color: "#1a1a1a"
                StatusHeader {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                }
            }

            Loader {
                id: pageLoader
                width: parent.width
                height: parent.height - 64
                source: nav.currentPage
                onSourceChanged: console.log("AppShell: loading", source)
                onStatusChanged: {
                    if (status === Loader.Error)
                        console.log("AppShell: FAILED to load", source)
                }
            }
        }
    }
}
