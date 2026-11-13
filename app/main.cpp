#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "core/NavController.h"
#include "core/PerfRecorder.h"
#include "core/RestClient.h"
#include "core/SseClient.h"
#include "core/StatusStore.h"
#include "core/SystemStats.h"
#include "core/TestVerdictStore.h"
#include "core/TimelineLoopCloser.h"
#include "core/UpgradeStore.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    auto *rest = new RestClient(&engine);
    // violoop-edge-llm is a separate same-machine service (:8000, not
    // proxied by violoop-edge) - see CLAUDE.md's note in edge-front2 and
    // TestLlmModelPage.qml, the only consumer.
    const QByteArray llmBaseEnv = qgetenv("EDGE_LLM_API_BASE");
    auto *llmRest = new RestClient(&engine, QString::fromLocal8Bit(
        llmBaseEnv.isEmpty() ? "http://127.0.0.1:8000" : llmBaseEnv));
    auto *sse = new SseClient(&engine);
    auto *status = new StatusStore(rest, &engine);
    auto *testVerdicts = new TestVerdictStore(rest, &engine);
    auto *upgrade = new UpgradeStore(rest, sse, &engine);
    auto *nav = new NavController(QStringLiteral("HomePage.qml"), &engine);
    auto *timelineLoopCloser = new TimelineLoopCloser(&engine);
    auto *systemStats = new SystemStats(&engine);
    auto *perfRecorder = new PerfRecorder(systemStats, &engine);

    engine.rootContext()->setContextProperty("nav", nav);
    engine.rootContext()->setContextProperty("restClient", rest);
    engine.rootContext()->setContextProperty("llmRestClient", llmRest);
    engine.rootContext()->setContextProperty("sseClient", sse);
    engine.rootContext()->setContextProperty("statusStore", status);
    engine.rootContext()->setContextProperty("testVerdictStore", testVerdicts);
    engine.rootContext()->setContextProperty("upgradeStore", upgrade);
    engine.rootContext()->setContextProperty("timelineLoopCloser", timelineLoopCloser);
    engine.rootContext()->setContextProperty("systemStats", systemStats);
    engine.rootContext()->setContextProperty("perfRecorder", perfRecorder);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/EdgeFrontQt/qml/AppShell.qml")));

    // For unattended runs (burn-in): open a page on top of the home page
    // right at startup, e.g. EDGE_KIOSK_OPEN_PAGE=AwayPage.qml, so the run
    // needs nobody to tap its way there - also not after the service has
    // restarted the app. Unset in normal use.
    const QByteArray openPage = qgetenv("EDGE_KIOSK_OPEN_PAGE");
    if (!openPage.isEmpty())
        nav->push(QString::fromLocal8Bit(openPage));

    // App-lifetime singletons started once here, same pattern as
    // ../../edge-front2/lib/app.dart: one SSE connection and one status
    // poller for the whole process, not re-created per page.
    sse->start();
    status->start();

    return app.exec();
}
