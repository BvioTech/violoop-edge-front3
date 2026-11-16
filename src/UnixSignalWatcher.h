#pragma once

#include <QObject>

class QSocketNotifier;

// Turns SIGUSR1 and SIGUSR2 into a Qt signal delivered on the event loop,
// so the app can be told to do something over SSH. What each one does is
// decided in main.cpp:
//
//   SIGUSR1  save a screenshot          (src/Screenshot.h)
//   SIGUSR2  start or stop the away page's performance recording
//            (src/PerfRecorder.h)
//
// Create one, before anything that could be signalled matters. The launch
// scripts ignore both signals until the app has started, so a signal sent
// too early is dropped instead of killing the service.
class UnixSignalWatcher : public QObject
{
    Q_OBJECT
public:
    explicit UnixSignalWatcher(QObject *parent = nullptr);

signals:
    // signalNumber is SIGUSR1 or SIGUSR2.
    void received(int signalNumber);

private:
    QSocketNotifier *m_notifier = nullptr;
};
