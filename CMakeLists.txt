cmake_minimum_required(VERSION 3.16)
project(violoop_edge_kiosk_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network Quick3D Test)

qt_add_executable(violoop_edge_kiosk_qt
    main.cpp
    src/ActionCatalog.cpp src/ActionCatalog.h
    src/ClipPlayer.cpp src/ClipPlayer.h
    src/JournalLog.cpp src/JournalLog.h
    src/MascotDirector.cpp src/MascotDirector.h
    src/MascotInputs.cpp src/MascotInputs.h
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
        qml/MascotScene.qml
        qml/PerfOverlay.qml
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
    RESOURCES
        # What the mascot model's clips are for, read by main.cpp for
        # src/MascotDirector (format: src/ActionCatalog.h).
        assets/away/actions.json
        # The designer's scene resources for qml/HomePage.qml, as shipped
        # in their package's qt/ folder: the environment light probe, the
        # face's own probe, the panorama the visor glass reflects, and the
        # glass shader, and the shader pair that draws the floor.
        assets/away/visor-studio.hdr
        assets/away/display-studio.hdr
        assets/away/optical-studio.hdr
        assets/away/canopy.frag
        assets/away/ground.vert
        assets/away/ground.frag
)

# The mascot model is deliberately NOT in RESOURCES above: it is BvioTech's
# proprietary asset, not under this program's GPL license (see
# packaging/debian/copyright), so it stays a file of its own that the
# program reads at run time instead of becoming part of the executable.
# main.cpp looks for it at assets/away/ beside the executable; this puts a
# copy there in the build directory, and scripts/build-deb.sh installs it
# the same way. Tracked via git-lfs (see .gitattributes).
# A git-lfs checkout without the LFS content leaves a small text pointer
# in the model's place. Copied as it is, that builds and starts fine and
# only shows as a mascot that fails to load, so stop here instead: a
# binary glTF file starts with the four bytes "glTF".
file(READ assets/away/VIO_DesktopPet.glb MASCOT_MODEL_MAGIC LIMIT 4 HEX)
if(NOT MASCOT_MODEL_MAGIC STREQUAL "676c5446")
    message(FATAL_ERROR
        "assets/away/VIO_DesktopPet.glb is not a binary glTF file - "
        "git-lfs content not fetched? (try `git lfs pull`)")
endif()
configure_file(assets/away/VIO_DesktopPet.glb assets/away/VIO_DesktopPet.glb COPYONLY)

target_link_libraries(violoop_edge_kiosk_qt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network Qt6::Quick3D
)

# Unit tests: `ctest --test-dir build`. Only what needs neither a screen
# nor Quick3D is tested this way; the pages are covered by the offscreen
# smoke test (README.md).
enable_testing()
qt_add_executable(test_mascot_director
    tests/test_mascot_director.cpp
    src/ActionCatalog.cpp src/ActionCatalog.h
    src/MascotDirector.cpp src/MascotDirector.h
)
target_link_libraries(test_mascot_director PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mascot_director COMMAND test_mascot_director)

qt_add_executable(test_mascot_inputs
    tests/test_mascot_inputs.cpp
    src/ActionCatalog.cpp src/ActionCatalog.h
    src/MascotDirector.cpp src/MascotDirector.h
    src/MascotInputs.cpp src/MascotInputs.h
)
target_link_libraries(test_mascot_inputs PRIVATE Qt6::Core Qt6::Test)
add_test(NAME mascot_inputs COMMAND test_mascot_inputs)
