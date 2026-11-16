#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "../src/MascotDirector.h"
#include "../src/MascotInputs.h"

// MascotInputs together with a real director: backend facts go in, the
// clips the director asks for come out.
class TestMascotInputs : public QObject
{
    Q_OBJECT

private:
    static constexpr int pollMs = 50;

    struct Fixture
    {
        MascotDirector director { QJsonDocument::fromJson(R"({
            "once": ["done"],
            "idle": { "base": "rest" },
            "work": ["type", "read"],
            "conditions": { "offline": "offline", "error": "error", "waiting_user": "wait" },
            "cues": { "done": ["done"] }
        })").object() };
        MascotInputs inputs { &director, pollMs };
        QSignalSpy played { &director, &MascotDirector::directive };
        QSignalSpy wanted { &inputs, &MascotInputs::snapshotWanted };

        Fixture()
        {
            director.stageAttached({ "rest", "offline", "wait", "type", "read", "error", "done" },
                                   { 250, 250, 250, 250, 250, 250, 250 });
        }
        QString clip() const { return played.last().at(0).toString(); }
        void authorizationEvent(const QString &state, const QString &reason = QString())
        {
            inputs.eventReceived("notification", QStringLiteral(
                R"({"type":"notification","kind":"authorization","state":"%1","notifId":"c1","data":{"reason":"%2"}})")
                .arg(state, reason));
        }
        // A goal.state event; `active` as JSON text.
        void goalEvent(const QString &state, const QString &active)
        {
            inputs.eventReceived("goal.state", QStringLiteral(
                R"({"type":"goal.state","goalId":"x","state":"%1","active":%2})").arg(state, active));
        }
        // How often the snapshot at `path` was asked for.
        int asked(const QString &path) const
        {
            int n = 0;
            for (const QList<QVariant> &call : wanted)
                n += call.at(0).toString() == path;
            return n;
        }
        void authorization(int statusCode, const QString &body)
        {
            inputs.snapshotReceived(MascotInputs::authorizationPath(), statusCode, body);
        }
    };

    static QString working(const QString &goal)
    {
        return QStringLiteral(R"({"goalId":"%1","state":"working","sessionId":"s"})").arg(goal);
    }
    static QString failed() { return R"({"goalId":"g1","state":"failed","sessionId":"s"})"; }
    static QString pending() { return R"({"active":{"context":"abc","expiresAt":1},"queue_length":0})"; }
    static QString none() { return R"({"active":null,"queue_length":0})"; }

private slots:
    void offlineAfterTwoFailedProbes()
    {
        Fixture f;
        f.inputs.connectivityProbed(false);
        QCOMPARE(f.clip(), QString("rest"));
        f.inputs.connectivityProbed(true);
        f.inputs.connectivityProbed(false);
        QCOMPARE(f.clip(), QString("rest"));
        f.inputs.connectivityProbed(false);
        QCOMPARE(f.clip(), QString("offline"));
        f.inputs.connectivityProbed(false);
        f.inputs.connectivityProbed(true);
        QCOMPARE(f.clip(), QString("rest"));
        QCOMPARE(f.played.size(), 3);
    }

    void asksForTheSnapshotWhenTheStreamConnects()
    {
        Fixture f;
        f.inputs.eventsConnected();
        QCOMPARE(f.asked(MascotInputs::authorizationPath()), 1);
        QCOMPARE(f.asked(MascotInputs::goalPath()), 1);
        f.authorization(200, pending());
        QCOMPARE(f.clip(), QString("wait"));
    }

    void openEventShowsAtOnceAndIsChecked()
    {
        Fixture f;
        f.authorizationEvent("open");
        QCOMPARE(f.clip(), QString("wait"));
        QCOMPARE(f.wanted.size(), 1);
        // The snapshot naming the request differently restarts nothing.
        f.authorization(200, pending());
        QCOMPARE(f.played.size(), 2);
    }

    void resolvedEventLeavesItToTheSnapshot()
    {
        Fixture f;
        f.authorizationEvent("open");
        // The button is no longer armed, the request still waits.
        f.authorizationEvent("resolved", "paused");
        f.authorization(200, pending());
        QCOMPARE(f.clip(), QString("wait"));
        f.authorizationEvent("resolved", "satisfied");
        f.authorization(200, none());
        QCOMPARE(f.clip(), QString("rest"));
    }

    void pollsOnlyWhileWaiting()
    {
        Fixture f;
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), 0);
        f.authorization(200, pending());
        QTRY_VERIFY(f.wanted.size() >= 2);
        // Resolved elsewhere, and no event came: the poll finds out.
        f.authorization(200, none());
        QCOMPARE(f.clip(), QString("rest"));
        const qsizetype asked = f.wanted.size();
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), asked);
    }

    void edgeGoneEndsTheWait()
    {
        Fixture f;
        f.authorization(200, pending());
        f.authorization(0, QString());
        QCOMPARE(f.clip(), QString("rest"));
        f.authorization(200, pending());
        f.authorization(503, R"({"error":"auth broker not initialized"})");
        QCOMPARE(f.clip(), QString("rest"));
    }

    void otherEventsAreIgnored()
    {
        Fixture f;
        f.inputs.eventReceived("notification", R"({"kind":"generic","state":"open"})");
        f.inputs.eventReceived("plan.created", R"({"plan_id":"p"})");
        f.inputs.eventReceived("notification", "not json");
        QCOMPARE(f.wanted.size(), 0);
        QCOMPARE(f.played.size(), 1);
    }

    void goalEventsSetWorkingAndError()
    {
        Fixture f;
        f.goalEvent("working", working("g1"));
        QCOMPARE(f.clip(), QString("type"));
        // Nothing to ask: the event carried the snapshot.
        QCOMPARE(f.wanted.size(), 0);
        f.goalEvent("failed", failed());
        QCOMPARE(f.clip(), QString("error"));
        // Retried: the same goal, the same clip.
        f.goalEvent("working", working("g1"));
        QCOMPARE(f.clip(), QString("type"));
        f.goalEvent("cancelled", "null");
        QCOMPARE(f.clip(), QString("rest"));
        f.goalEvent("working", working("g2"));
        QCOMPARE(f.clip(), QString("read"));
        f.goalEvent("failed", failed());
        f.goalEvent("resolved", "null");
        QCOMPARE(f.clip(), QString("rest"));
    }

    void doneCelebratesOnce()
    {
        Fixture f;
        f.goalEvent("working", working("g1"));
        f.goalEvent("done", "null");
        QCOMPARE(f.clip(), QString("done"));
        QTRY_COMPARE(f.clip(), QString("rest"));
        // The snapshot never celebrates.
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200, R"({"active":null})");
        QCOMPARE(f.clip(), QString("rest"));
        QCOMPARE(f.played.size(), 4);
    }

    void doneWithTheNextGoalAlreadyRunning()
    {
        Fixture f;
        f.goalEvent("working", working("g1"));
        f.goalEvent("done", working("g2"));
        QCOMPARE(f.clip(), QString("done"));
        QTRY_COMPARE(f.clip(), QString("read"));
    }

    void goalSnapshotIsPolledWhileActive()
    {
        Fixture f;
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200,
                                  QStringLiteral(R"({"active":%1})").arg(working("g1")));
        QCOMPARE(f.clip(), QString("type"));
        QTRY_VERIFY(f.asked(MascotInputs::goalPath()) >= 2);
        QCOMPARE(f.asked(MascotInputs::authorizationPath()), 0);
        // violoop-edge restarted: its goals are gone.
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 0, QString());
        QCOMPARE(f.clip(), QString("rest"));
        const qsizetype asked = f.wanted.size();
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), asked);
    }

    void waitingOutranksWorking()
    {
        Fixture f;
        f.goalEvent("working", working("g1"));
        f.authorization(200, pending());
        QCOMPARE(f.clip(), QString("wait"));
        f.authorization(200, none());
        QCOMPARE(f.clip(), QString("type"));
    }

    void offlineOutranksWaiting()
    {
        Fixture f;
        f.authorization(200, pending());
        f.inputs.connectivityProbed(false);
        f.inputs.connectivityProbed(false);
        QCOMPARE(f.clip(), QString("offline"));
        f.inputs.connectivityProbed(true);
        QCOMPARE(f.clip(), QString("wait"));
    }
};

QTEST_GUILESS_MAIN(TestMascotInputs)
#include "test_mascot_inputs.moc"
