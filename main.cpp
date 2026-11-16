#include <QDebug>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

#include "src/ClipPlayer.h"
#include "src/JournalLog.h"
#include "src/MascotDirector.h"
#include "src/MascotInputs.h"
#include "src/NavController.h"
#include "src/PerfRecorder.h"
#include "src/RestClient.h"
#include "src/Screenshot.h"
#include "src/SseClient.h"
#include "src/StatusStore.h"
#include "src/SystemStats.h"
#include "src/TestVerdictStore.h"
#include "src/UnixSignalWatcher.h"
#include "src/UpgradeStore.h"

#include <signal.h>

int main(int argc, char *argv[])
{
    // Before QGuiApplication, so platform plugin warnings get a level too.
    installJournalLogHandler();

    QGuiApplication app(argc, argv);

    // Ask for the same font family whichever way the app reaches the
    // screen. Left alone, the wayland plugin's default is "Sans Serif"
    // (Noto Sans on the device) but eglfs's is DejaVu, which is wider and
    // would shift every page's layout. "Sans Serif" keeps what the pages
    // were laid out with under weston.
    QFont font = app.font();
    font.setFamily(QStringLiteral("Sans Serif"));
    app.setFont(font);

    // Instantiated by qml/MascotScene.qml, one per loaded model.
    qmlRegisterType<ClipPlayer>("EdgeFrontQt.Core", 1, 0, "ClipPlayer");
    // For MascotDirector's enums (Mascot.Working, Mascot.TouchLeft, ...);
    // the one instance is `mascot`, below.
    qmlRegisterUncreatableType<MascotDirector>("EdgeFrontQt.Core", 1, 0, "Mascot",
                                               QStringLiteral("use the `mascot` context property"));

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
    auto *systemStats = new SystemStats(&engine);
    auto *perfRecorder = new PerfRecorder(systemStats, &engine);
    // Decides which clip the home page's mascot plays, see
    // MascotDirector.h. The manifest says what the built-in model's
    // clips are for.
    QFile manifestFile(QStringLiteral(":/EdgeFrontQt/assets/away/actions.json"));
    QJsonObject manifest;
    if (manifestFile.open(QIODevice::ReadOnly))
        manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
    if (manifest.isEmpty())
        qWarning() << "main: cannot read the mascot's action manifest" << manifestFile.fileName();
    auto *mascot = new MascotDirector(manifest, &engine);
    // What the backend knows, turned into the director's conditions, see
    // MascotInputs.h.
    auto *mascotInputs = new MascotInputs(mascot, 5000, &engine);
    QObject::connect(status, &StatusStore::connectivityChanged, mascotInputs,
                     [mascotInputs, status]() { mascotInputs->connectivityProbed(status->connectivityOk()); });
    QObject::connect(sse, &SseClient::connected, mascotInputs, &MascotInputs::eventsConnected);
    QObject::connect(sse, &SseClient::eventReceived, mascotInputs, &MascotInputs::eventReceived);
    QObject::connect(mascotInputs, &MascotInputs::snapshotWanted, rest, &RestClient::get);
    QObject::connect(rest, &RestClient::requestSucceeded, mascotInputs, &MascotInputs::snapshotReceived);
    QObject::connect(rest, &RestClient::requestFailed, mascotInputs,
                     [mascotInputs](const QString &path) { mascotInputs->snapshotReceived(path, 0, QString()); });
    // SIGUSR1 saves a picture of the window, SIGUSR2 starts or stops the
    // away page's performance recording - see UnixSignalWatcher.h.
    auto *signalWatcher = new UnixSignalWatcher(&engine);
    QObject::connect(signalWatcher, &UnixSignalWatcher::received, perfRecorder, [perfRecorder](int signalNumber) {
        if (signalNumber == SIGUSR1)
            saveScreenshot();
        else if (signalNumber == SIGUSR2)
            perfRecorder->toggle();
    });
    // For unattended runs (burn-in): record from the start, so that a
    // restart of the service does not silently end the recording.
    if (qEnvironmentVariableIntValue("EDGE_KIOSK_RECORD_PERF"))
        perfRecorder->start();

    engine.rootContext()->setContextProperty("nav", nav);
    engine.rootContext()->setContextProperty("restClient", rest);
    engine.rootContext()->setContextProperty("llmRestClient", llmRest);
    engine.rootContext()->setContextProperty("sseClient", sse);
    engine.rootContext()->setContextProperty("statusStore", status);
    engine.rootContext()->setContextProperty("testVerdictStore", testVerdicts);
    engine.rootContext()->setContextProperty("upgradeStore", upgrade);
    engine.rootContext()->setContextProperty("systemStats", systemStats);
    engine.rootContext()->setContextProperty("perfRecorder", perfRecorder);
    engine.rootContext()->setContextProperty("mascot", mascot);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/EdgeFrontQt/qml/AppShell.qml")));

    // For unattended runs: open a page on top of the home page right at
    // startup, e.g. EDGE_KIOSK_OPEN_PAGE=TestDisplayPage.qml, so the run
    // needs nobody to tap its way there - also not after the service has
    // restarted the app. Unset in normal use.
    const QByteArray openPage = qgetenv("EDGE_KIOSK_OPEN_PAGE");
    if (!openPage.isEmpty())
        nav->push(QString::fromLocal8Bit(openPage));

    // App-lifetime singletons started once here, same pattern as
    // ../edge-front2/lib/app.dart: one SSE connection and one status
    // poller for the whole process, not re-created per page.
    sse->start();
    status->start();

    return app.exec();
}
