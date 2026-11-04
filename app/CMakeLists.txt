cmake_minimum_required(VERSION 3.16)
project(violoop_edge_kiosk_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

qt_add_executable(violoop_edge_kiosk_qt
    main.cpp
    core/RestClient.cpp core/RestClient.h
    core/SseClient.cpp core/SseClient.h
    core/StatusStore.cpp core/StatusStore.h
    core/TestVerdictStore.cpp core/TestVerdictStore.h
    core/UpgradeStore.cpp core/UpgradeStore.h
)

qt_add_qml_module(violoop_edge_kiosk_qt
    URI EdgeFrontQt
    VERSION 1.0
    QML_FILES
        qml/AppShell.qml
        qml/Nav.qml
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
)

target_link_libraries(violoop_edge_kiosk_qt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
)
