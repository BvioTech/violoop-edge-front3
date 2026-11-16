#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class MascotDirector;

// Turns what the rest of the app knows into the director's conditions.
// It is the only place that knows which backend fact stands for which
// condition; the director knows none of them (MascotDirector.h).
//
// It talks to nothing itself: main.cpp feeds it from StatusStore,
// SseClient and RestClient, and sends its one request out. That keeps it
// testable without a network.
//
// What is wired so far:
//
//   - Offline: the cloud probe (GET /status/connectivity, polled by
//     StatusStore) failed twice in a row; over with the first probe that
//     succeeds. Twice, because one lost ping is not worth the grey eyes.
//     Not being able to reach violoop-edge itself is a different matter
//     and does not count: StatusStore then has no new probe to report.
//   - WaitingUser: an authorization request is waiting for the user
//     (GET /sign/authorize-status has an `active` entry). The
//     `notification` events of kind `authorization` only say when to look:
//     they follow the device button being armed, which also ends for
//     reasons that leave the request waiting (`paused`, `error`), and the
//     event stream may drop frames. So the snapshot is asked for when the
//     stream (re)connects, on every such event, and every few seconds for
//     as long as a request is waiting.
//
// Working, Done and Error wait for events violoop-edge does not send yet,
// see docs/handoff-device-mascot-events.md.
class MascotInputs : public QObject
{
    Q_OBJECT

public:
    // `authorizationPollMs`: how often the snapshot is asked for again
    // while a request is waiting.
    explicit MascotInputs(MascotDirector *director, int authorizationPollMs = 5000,
                          QObject *parent = nullptr);

    // The path authorizationStatusWanted() is about.
    static QString authorizationStatusPath() { return QStringLiteral("/sign/authorize-status"); }

    // A cloud probe came back.
    void connectivityProbed(bool ok);

    // The event stream is up (again).
    void eventsConnected();
    // An event from the stream; `data` is its JSON text.
    void eventReceived(const QString &type, const QString &data);

    // The answer to authorizationStatusWanted(). `statusCode` 0: there
    // was none.
    void authorizationStatusReceived(int statusCode, const QString &body);

signals:
    // Somebody please GET authorizationStatusPath() and report back.
    void authorizationStatusWanted();

private:
    void setWaiting(bool waiting, const QString &request = QString());

    MascotDirector *m_director;
    int m_failedProbes = 0;
    QTimer m_authorizationPoll;
};
