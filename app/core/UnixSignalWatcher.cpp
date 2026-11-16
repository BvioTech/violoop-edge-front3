#include "UnixSignalWatcher.h"

#include <QDebug>
#include <QSocketNotifier>

#include <cerrno>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

// A signal handler may only do async-signal-safe work, so it writes the
// signal's number to this pipe and the event loop reads it from the other
// end.
int g_pipe[2] = {-1, -1};

void onSignal(int signalNumber)
{
    // write() may change errno, and the code this signal interrupted may
    // be about to read it.
    const int savedErrno = errno;
    const char byte = static_cast<char>(signalNumber);
    const ssize_t written = ::write(g_pipe[1], &byte, 1);
    Q_UNUSED(written);
    errno = savedErrno;
}

} // namespace

UnixSignalWatcher::UnixSignalWatcher(QObject *parent)
    : QObject(parent)
{
    if (::pipe2(g_pipe, O_CLOEXEC | O_NONBLOCK) != 0) {
        qWarning() << "UnixSignalWatcher: pipe2 failed, SIGUSR1 and SIGUSR2 do nothing";
        return;
    }
    m_notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this]() {
        char buffer[16];
        ssize_t count;
        while ((count = ::read(g_pipe[0], buffer, sizeof buffer)) > 0) {
            for (ssize_t i = 0; i < count; ++i)
                emit received(buffer[i]);
        }
    });

    struct sigaction action = {};
    action.sa_handler = onSignal;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);
    for (int signalNumber : {SIGUSR1, SIGUSR2}) {
        if (::sigaction(signalNumber, &action, nullptr) != 0)
            qWarning() << "UnixSignalWatcher: sigaction failed for signal" << signalNumber;
    }
}
