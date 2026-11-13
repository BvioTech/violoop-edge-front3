cmake_minimum_required(VERSION 3.16)
project(violoop_edge_kiosk_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Quick3D)

qt_add_executable(violoop_edge_kiosk_qt
    main.cpp
    core/NavController.cpp core/NavController.h
    core/RestClient.cpp core/RestClient.h
    core/SseClient.cpp core/SseClient.h
    core/StatusStore.cpp core/StatusStore.h
    core/TestVerdictStore.cpp core/TestVerdictStore.h
    core/TimelineLoopCloser.cpp core/TimelineLoopCloser.h
    core/UpgradeStore.cpp core/UpgradeStore.h
)

qt_add_qml_module(violoop_edge_kiosk_qt
    URI EdgeFrontQt
    VERSION 1.0
    QML_FILES
        qml/AppShell.qml
        qml/StatusHeader.qml
        qml/StatusDot.qml
        qml/TestPageFrame.qml
        qml/ActionCard.qml
        qml/WifiScanList.qml
        qml/HomePage.qml
        qml/DevMenuPage.qml
        qml/TestLandingPage.qml
        qml/TestDefinitions.js
        qml/TestDeviceInfoPage.qml
        qml/TestMemoryInfoPage.qml
        qml/TestSnInfoPage.qml
        qml/TestDisplayPage.qml
        qml/TestTouchPage.qml
        qml/TestBacklightPage.qml
        qml/TestFanPage.qml
        qml/TestLightPage.qml
        qml/TestAudioPage.qml
        qml/TestAudioEchoPage.qml
        qml/TestMcuPingPage.qml
        qml/TestButtonPage.qml
        qml/TestWifiPage.qml
        qml/TestBluetoothPage.qml
        qml/TestPowerPage.qml
        qml/TestUsbPage.qml
        qml/TestUsbInputPage.qml
        qml/TestHdmiPage.qml
        qml/TestComputeCardPage.qml
        qml/TestLlmModelPage.qml
        qml/TestStressPlaceholderPage.qml
        qml/DebugPage.qml
        qml/DebugTouchPage.qml
        qml/DebugWifiPage.qml
        qml/VersionPage.qml
        qml/AwayPage.qml
    RESOURCES
        # The designer's mascot model, loaded by qml/AwayPage.qml. Tracked
        # via git-lfs (see ../.gitattributes), not fetched by a script like
        # qt3d-spike's assets: it ships inside the installed app, so it has
        # to be a real committed asset, not a dev-time-only download.
        assets/away/VIO_DesktopPet.glb
        # The placeholder the page used before that (Khronos
        # glTF-Sample-Models' CesiumMan, CC0). Kept as the lightweight
        # baseline every real-hardware number so far was measured with.
        assets/away/CesiumMan.glb
        # Light probe for qml/AwayPage.qml, written by
        # scripts/make-away-probe.py.
        assets/away/studio-probe.hdr
)

target_link_libraries(violoop_edge_kiosk_qt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Quick3D
)
