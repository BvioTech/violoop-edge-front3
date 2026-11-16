#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "perf_monitor.h"
#include "rest_client.h"
#include "sse_client.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Which compiled scene to load (see docs/findings.md for what each
    // validation item covers): "rotation_touch" (A), "voice_state" (D),
    // "cjk_text" (E), or the default "combined" (B/C/D/A/F all running
    // together, for the real-device soak test). Unlike qt3d-spike (one
    // fixed QML entry point + a separate qml_runner.cpp for ad-hoc files),
    // all scenes here are compiled into the same qt_add_qml_module, so
    // switching between them is just an argv pick.
    QString scene = QStringLiteral("combined");
    const QStringList args = app.arguments();
    if (args.size() > 1 && !args.at(1).startsWith('-'))
        scene = args.at(1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("perfMonitor", new PerfMonitor(&engine));
    engine.rootContext()->setContextProperty("restClient", new RestClient(&engine));
    engine.rootContext()->setContextProperty("sseClient", new SseClient(&engine));
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/UiInterfaceSpike/%1.qml").arg(scene)));

    return app.exec();
}
