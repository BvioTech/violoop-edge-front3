cmake_minimum_required(VERSION 3.16)
project(violoop_edge_kiosk_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Quick3D)

qt_add_executable(violoop_edge_kiosk_qt
    main.cpp
    src/ClipPlayer.cpp src/ClipPlayer.h
    src/JournalLog.cpp src/JournalLog.h
    src/NavController.cpp src/NavController.h
    src/RestClient.cpp src/RestClient.h
    src/Screenshot.cpp src/Screenshot.h
    src/SseClient.cpp src/SseClient.h
    src/StatusStore.cpp src/StatusStore.h
    src/TestVerdictStore.cpp src/TestVerdictStore.h
    src/StateDir.cpp src/StateDir.h
    src/SystemStats.cpp src/SystemStats.h
    src/PerfRecorder.cpp src/PerfRecorder.h
    src/UnixSignalWatcher.cpp src/UnixSignalWatcher.h
    src/UpgradeStore.cpp src/UpgradeStore.h
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
        qml/AwayScene.qml
        qml/PerfOverlay.qml
    RESOURCES
        # The designer's mascot model, loaded by qml/AwayPage.qml. Tracked
        # via git-lfs (see .gitattributes), not fetched by a script like
        # demos/qt3d-spike's assets: it ships inside the installed app, so it has
        # to be a real committed asset, not a dev-time-only download.
        assets/away/VIO_DesktopPet.glb
        # The designer's scene resources for qml/AwayPage.qml, as shipped
        # in their package's qt/ folder: the environment light probe, the
        # face's own probe, the panorama the visor glass reflects, and the
        # glass shader.
        assets/away/visor-studio.hdr
        assets/away/display-studio.hdr
        assets/away/optical-studio.hdr
        assets/away/canopy.frag
)

target_link_libraries(violoop_edge_kiosk_qt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Quick3D
)
