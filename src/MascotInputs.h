#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

class MascotDirector;

// Turns what the rest of the app knows into the director's conditions
// and cues. It is the only place that knows which backend fact stands for
// which of them; the director knows none (MascotDirector.h).
//
// It talks to nothing itself: main.cpp feeds it from StatusStore,
// SseClient and RestClient, and sends its requests out. That keeps it
// testable without a network.
//
// The event stream may drop frames and replays nothing (see
// ../../device/docs/protocols/local-http-events.md), so for whatever
// lasts, the truth is a snapshot fetched over REST and the events only
// say when to look: the snapshots are asked for when the stream
// (re)connects, on the events named below, and every few seconds for as
// long as they say that something is going on.
//
//   - Offline: the cloud probe (GET /status/connectivity, polled by
//     StatusStore) failed twice in a row; over with the first probe that
//     succeeds. Twice, because one lost ping is not worth the grey eyes.
//     Not being able to reach violoop-edge itself is a different matter
//     and does not count: StatusStore then has no new probe to report.
//   - WaitingUser: an authorization request is waiting for the user
//     (GET /sign/authorize-status has an `active` entry). The
//     `notification` events of kind `authorization` follow the device
//     button being armed, which also ends for reasons that leave the
//     request waiting (`paused`, `error`) - hence only a hint.
//   - Working, Error and the Done cue: violoop-edge's goals (`goal.state`
//     events, GET /status/goal). A goal is one thing the user asked for
//     that takes doing; `active` in the event and in the snapshot is the
//     goal being worked on, or the failure nobody has dealt with yet.
//     Every event carries `active`, so an event is as good as a snapshot.
//     `done` is the one thing that is only ever an event: it throws the
//     confetti, once, and is never made up for afterwards.
class MascotInputs : public QObject
{
    Q_OBJECT

public:
    // `pollMs`: how often a snapshot is asked for again while it says
    // that something is going on.
    explicit MascotInputs(MascotDirector *director, int pollMs = 5000, QObject *parent = nullptr);

    static QString authorizationPath() { return QStringLiteral("/sign/authorize-status"); }
    static QString goalPath() { return QStringLiteral("/status/goal"); }

    // A cloud probe came back.
    void connectivityProbed(bool ok);

    // The event stream is up (again).
    void eventsConnected();
    // An event from the stream; `data` is its JSON text.
    void eventReceived(const QString &type, const QString &data);

    // The answer to snapshotWanted(path). `statusCode` 0: there was none.
    // Answers for other paths are ignored.
    void snapshotReceived(const QString &path, int statusCode, const QString &body);

signals:
    // Somebody please GET `path` and report back.
    void snapshotWanted(const QString &path);

private:
    void setWaiting(bool waiting, const QString &request = QString());
    void setGoal(const QJsonObject &active);
    void updatePoll();

    MascotDirector *m_director;
    int m_failedProbes = 0;
    bool m_waiting = false;
    bool m_goalActive = false;
    QTimer m_poll;
};
