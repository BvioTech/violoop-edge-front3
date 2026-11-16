cmake_minimum_required(VERSION 3.16)
project(qt3d_spike LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Quick3D)

qt_add_executable(qt3d_spike main.cpp perf_monitor.cpp perf_monitor.h)
qt_add_qml_module(qt3d_spike
    URI Qt3dSpike
    VERSION 1.0
    QML_FILES runtime_loader.qml
)

target_link_libraries(qt3d_spike PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D
)
