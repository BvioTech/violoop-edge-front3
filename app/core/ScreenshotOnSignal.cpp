#include "ScreenshotOnSignal.h"

#include "UnixSignalWatcher.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTransform>

#include <signal.h>

ScreenshotOnSignal::ScreenshotOnSignal(UnixSignalWatcher *watcher, QObject *parent)
    : QObject(parent)
{
    connect(watcher, &UnixSignalWatcher::received, this, [this](int signalNumber) {
        if (signalNumber == SIGUSR1)
            take();
    });
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
