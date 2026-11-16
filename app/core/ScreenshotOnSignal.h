#pragma once

#include <QObject>

class UnixSignalWatcher;

// Saves a picture of the app's window when the process gets SIGUSR1, so a
// screen can be captured over SSH:
//
//   sudo kill -USR1 "$(systemctl show -p MainPID --value violoop-edge-kiosk-qt)"
//
// The picture goes to the service's state directory
// (/var/lib/violoop-edge-kiosk-qt/screenshot-<date>-<time>.png, or the
// temp directory when run by hand) and the path is logged at info level.
//
// The app takes the picture itself, rather than a compositor tool like
// weston-screenshooter, so that it also works under eglfs, where there is
// no compositor to ask.
class ScreenshotOnSignal : public QObject
{
    Q_OBJECT
public:
    explicit ScreenshotOnSignal(UnixSignalWatcher *watcher, QObject *parent = nullptr);

private:
    void take();
};
