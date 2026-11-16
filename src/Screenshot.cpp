#include "Screenshot.h"

#include "StateDir.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QTransform>

void saveScreenshot()
{
    QQuickWindow *window = nullptr;
    const QWindowList windows = QGuiApplication::topLevelWindows();
    for (QWindow *candidate : windows) {
        window = qobject_cast<QQuickWindow *>(candidate);
        if (window)
            break;
    }
    if (!window) {
        qWarning() << "Screenshot: no window to capture";
        return;
    }

    QImage image = window->grabWindow();
    if (image.isNull()) {
        qWarning() << "Screenshot: grabWindow returned nothing";
        return;
    }
    // qml/AppShell.qml turns the whole interface by -90 degrees to suit
    // how the panel is mounted. Turn the picture back so it reads upright.
    image = image.transformed(QTransform().rotate(90));

    const QString path = QDir(stateDirectory()).filePath(QStringLiteral("screenshot-%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))));
    if (!image.save(path)) {
        qWarning() << "Screenshot: could not write" << path;
        return;
    }
    qInfo() << "Screenshot: saved" << path;
}
