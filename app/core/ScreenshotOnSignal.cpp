#include "ScreenshotOnSignal.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTransform>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

// A signal handler may only do async-signal-safe work, so it writes one
// byte to this pipe and the event loop picks it up from the other end.
int g_pipe[2] = {-1, -1};

void onSignal(int)
{
    const char byte = 1;
    const ssize_t written = ::write(g_pipe[1], &byte, 1);
    Q_UNUSED(written);
}

} // namespace

ScreenshotOnSignal::ScreenshotOnSignal(QObject *parent)
    : QObject(parent)
{
    if (::pipe2(g_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        qWarning() << "ScreenshotOnSignal: pipe2 failed, screenshots on SIGUSR1 are off";
        return;
    }
    m_notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this]() {
        char buffer[16];
        while (::read(g_pipe[0], buffer, sizeof buffer) > 0) { }
        take();
    });

    struct sigaction action = {};
    action.sa_handler = onSignal;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    if (::sigaction(SIGUSR1, &action, nullptr) != 0)
        qWarning() << "ScreenshotOnSignal: sigaction failed, screenshots on SIGUSR1 are off";
}

void ScreenshotOnSignal::take()
{
    QQuickWindow *window = nullptr;
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *candidate : windows) {
        window = qobject_cast<QQuickWindow *>(candidate);
        if (window)
            break;
    }
    if (!window) {
        qWarning() << "ScreenshotOnSignal: no window to capture";
        return;
    }

    QImage image = window->grabWindow();
    if (image.isNull()) {
        qWarning() << "ScreenshotOnSignal: grabWindow returned nothing";
        return;
    }
    // qml/AppShell.qml turns the whole interface by -90 degrees to suit
    // how the panel is mounted. Turn the picture back so it reads upright.
    image = image.transformed(QTransform().rotate(90));

    // systemd may list several directories, colon-separated; ours has one.
    QString dir = QString::fromLocal8Bit(qgetenv("STATE_DIRECTORY")).section(QLatin1Char(':'), 0, 0);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString path = QDir(dir).filePath(QStringLiteral("screenshot-%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
    if (!image.save(path)) {
        qWarning() << "ScreenshotOnSignal: could not write" << path;
        return;
    }
    qInfo() << "ScreenshotOnSignal: saved" << path;
}
