cmake_minimum_required(VERSION 3.16)
project(ui_interface_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

qt_add_executable(ui_interface_spike
    main.cpp
    perf_monitor.cpp perf_monitor.h
    rest_client.cpp rest_client.h
    sse_client.cpp sse_client.h
)
qt_add_qml_module(ui_interface_spike
    URI UiInterfaceSpike
    VERSION 1.0
    QML_FILES
        RotationTouchArea.qml
        VoiceStateView.qml
        CjkTextSample.qml
        rotation_touch.qml
        voice_state.qml
        cjk_text.qml
        combined.qml
)

target_link_libraries(ui_interface_spike PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
)
