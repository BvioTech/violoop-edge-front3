#pragma once

#include <QDeadlineTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
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
// long as they say that something is going on. An answer that was
// overtaken by an event is not used: the event is the newer of the two,
// and the snapshot is asked for once more.
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
//     Every event carries what the snapshot has, so an event is as good
//     as a snapshot. `last` in both is the goal that ended last: when
//     that is a `done` not seen before, the confetti is thrown - so a
//     `done` event that got lost is made up for by the next snapshot.
//     Not by the first one after the app started, though: what that
//     shows ended before anybody was looking. When the director has no
//     use for the confetti right then (a touch reaction playing, the page
//     hidden, a conversation going on), it is offered again for a while.
//   - Conversation: the realtime voice session's phase (`realtime:state`
//     events, GET /audio/realtime/status): connecting is "preparing",
//     thinking and working (a tool running) are both "thinking",
//     listening and speaking are themselves. `ended`, or no session in
//     the snapshot, ends it.
class MascotInputs : public QObject
{
    Q_OBJECT

public:
    // `pollMs`: how often a snapshot is asked for again while it says
    // that something is going on.
    explicit MascotInputs(MascotDirector *director, int pollMs = 5000, QObject *parent = nullptr);

    // For how long, and how often, confetti the director had no use for
    // is offered again.
    void setCelebrationRetry(int forMs, int everyMs);

    static QString authorizationPath() { return QStringLiteral("/sign/authorize-status"); }
    static QString goalPath() { return QStringLiteral("/status/goal"); }
    static QString conversationPath() { return QStringLiteral("/audio/realtime/status"); }

    // A cloud probe came back.
    void connectivityProbed(bool ok);

    // The event stream is up (again).
    void eventsConnected();
    // An event from the stream; `data` is its JSON text.
    void eventReceived(const QString &type, const QString &data);

    // The answer to snapshotWanted(path). `statusCode` 0: there was none.
    // Answers for other paths are ignored.
    void snapshotReceived(const QString &path, int statusCode, const QString &body);

    // Shows this phase of a voice session (the backend's name for it)
    // from now on, whatever the backend says. For trying the phases out.
    void pinConversation(const QString &state);

signals:
    // Somebody please GET `path` and report back.
    void snapshotWanted(const QString &path);

private:
    void ask(const QString &path);
    void overtaken(const QString &path);
    void setWaiting(bool waiting, const QString &request = QString());
    void setGoal(const QJsonObject &active);
    bool noteLastGoal(const QJsonObject &last);
    void celebrate(bool fresh);
    void setConversation(const QString &state);
    void showConversation(const QString &state);
    void updatePoll();

    MascotDirector *m_director;
    int m_failedProbes = 0;
    bool m_waiting = false;
    bool m_goalActive = false;
    bool m_conversing = false;
    bool m_conversationPinned = false;
    QTimer m_poll;

    // Per snapshot path: requests not answered yet, and whether an event
    // on the same subject arrived while there were any.
    QHash<QString, int> m_asked;
    QSet<QString> m_overtaken;

    // The goals seen to have ended (id and end time), newest last, and
    // whether anything has been seen at all.
    QStringList m_endedGoals;
    bool m_lastGoalKnown = false;
    // Confetti still owed, and for how much longer.
    QTimer m_celebrationRetry;
    QDeadlineTimer m_celebrationDeadline;
    int m_celebrationForMs = 60000;
};
