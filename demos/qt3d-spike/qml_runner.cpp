// Small standalone QML runner: loads an arbitrary .qml file from disk by
// path, instead of a resource baked in at build time. Debian's
// qt6-declarative-dev-tools does not package the generic `qml` script
// runner, so this stands in for it — useful for quick ad-hoc scenes
// (see primitive_test.qml) without touching CMakeLists.txt / rebuilding
// qt3d_spike's resource bundle for every experiment.
//
// Build directly with g++ (no CMake needed), e.g. on the target device:
//   g++ -std=c++17 -fPIC \
//     -I/usr/include/aarch64-linux-gnu/qt6 \
//     -I/usr/include/aarch64-linux-gnu/qt6/QtCore \
//     -I/usr/include/aarch64-linux-gnu/qt6/QtGui \
//     -I/usr/include/aarch64-linux-gnu/qt6/QtQml \
//     -I/usr/include/aarch64-linux-gnu/qt6/QtQuick \
//     -I/usr/include/aarch64-linux-gnu/qt6/QtQuick3D \
//     qml_runner.cpp -o qml_runner \
//     -lQt6Core -lQt6Gui -lQt6Qml -lQt6Quick -lQt6Quick3D
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QSurfaceFormat>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // See main.cpp's comment: this has no observed effect on this Qt
    // 6.4.2 / libmali combination, kept for parity with qt3d_spike.
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(3, 2);
    QSurfaceFormat::setDefaultFormat(fmt);

    if (argc < 2) {
        qFatal("usage: qml_runner <file.qml>");
    }

    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
