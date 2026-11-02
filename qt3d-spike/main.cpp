#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSurfaceFormat>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Quick3D's default material shaders use textureSize(), which needs
    // GLSL ES >= 3.00 (ESSL 100 / GLES2 fails to compile it). The libmali
    // r24p0 blob reports GLES 3.2 (confirmed via glGetString at runtime —
    // see docs/findings.md), but on this Qt 6.4.2 the context Quick3D's
    // shader codegen sees is still reported as version 2.0 either way —
    // this call (tried both before and after the QGuiApplication
    // constructor) makes no observable difference on either the
    // wayland-egl or eglfs_kms QPA backend. Left in as documentation of
    // what was tried, not as a working fix — see findings.md "蒙皮动画限制".
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(3, 2);
    QSurfaceFormat::setDefaultFormat(fmt);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/Qt3dSpike/runtime_loader.qml")));

    return app.exec();
}
