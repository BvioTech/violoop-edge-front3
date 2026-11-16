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
            "once": ["done", "welcome"],
            "idle": { "base": "rest" },
            "work": ["type", "read"],
            "conditions": { "offline": "offline", "error": "error", "waiting_user": "wait" },
            "cues": { "done": ["done"], "welcome": ["welcome"] }
        })").object() };
        MascotInputs inputs { &director, pollMs };
        QSignalSpy played { &director, &MascotDirector::directive };
        QSignalSpy wanted { &inputs, &MascotInputs::snapshotWanted };

        Fixture()
        {
            director.stageAttached({ "rest", "offline", "wait", "type", "read", "error", "done", "welcome" },
                                   { 250, 250, 250, 250, 250, 250, 250, 250 });
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
        QCOMPARE(f.asked(MascotInputs::conversationPath()), 1);
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
        f.authorization(200, pending());
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
        QCOMPARE(f.played.size(), 5);   // rest, type, rest, done, rest
    }

    void lostDoneIsMadeUpForByTheSnapshot()
    {
        Fixture f;
        auto snapshot = [&f](const QString &active, const QString &last) {
            f.inputs.snapshotReceived(MascotInputs::goalPath(), 200,
                                      QStringLiteral(R"({"active":%1,"last":%2})").arg(active, last));
        };
        const QString done1 = R"({"goalId":"g1","state":"done","time":100})";
        const QString done2 = R"({"goalId":"g2","state":"done","time":200})";
        // What ended before the app started is not celebrated.
        snapshot("null", done1);
        QCOMPARE(f.played.size(), 1);
        snapshot(working("g2"), done1);
        QCOMPARE(f.clip(), QString("type"));
        // The done event for g2 never came.
        snapshot("null", done2);
        QCOMPARE(f.clip(), QString("done"));
        QTRY_COMPARE(f.clip(), QString("rest"));
        snapshot("null", done2);
        QCOMPARE(f.played.size(), 5);
        // A cancelled goal is remembered, not celebrated.
        snapshot("null", R"({"goalId":"g3","state":"cancelled","time":300})");
        QCOMPARE(f.played.size(), 5);
    }

    void doneEventAndSnapshotCelebrateOnce()
    {
        Fixture f;
        const QString done1 = R"({"goalId":"g1","state":"done","time":100})";
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200, R"({"active":null,"last":null})");
        f.inputs.eventReceived("goal.state", QStringLiteral(
            R"({"goalId":"g1","state":"done","active":null,"last":%1})").arg(done1));
        QCOMPARE(f.clip(), QString("done"));
        QTRY_COMPARE(f.clip(), QString("rest"));
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200,
                                  QStringLiteral(R"({"active":null,"last":%1})").arg(done1));
        // Another goal's event still carries the old `last`.
        f.inputs.eventReceived("goal.state", QStringLiteral(
            R"({"goalId":"g2","state":"cancelled","active":null,"last":%1})").arg(done1));
        QCOMPARE(f.played.size(), 3);
    }

    void eventBeforeTheFirstSnapshot()
    {
        Fixture f;
        // Not a done event: its `last` is from before the app started.
        f.inputs.eventReceived("goal.state",
            R"({"goalId":"g2","state":"working","active":null,"last":{"goalId":"g1","state":"done","time":100}})");
        QCOMPARE(f.played.size(), 1);
        f.inputs.eventReceived("goal.state",
            R"({"goalId":"g2","state":"done","active":null,"last":{"goalId":"g2","state":"done","time":200}})");
        QCOMPARE(f.clip(), QString("done"));
    }

    void staleAnswerIsNotUsed()
    {
        Fixture f;
        f.inputs.eventsConnected();
        // The event overtakes the answer to the request just made.
        f.goalEvent("working", working("g1"));
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200, R"({"active":null,"last":null})");
        QCOMPARE(f.clip(), QString("type"));
        // Asked again, and that answer counts.
        QCOMPARE(f.asked(MascotInputs::goalPath()), 2);
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200, R"({"active":null,"last":null})");
        QCOMPARE(f.clip(), QString("rest"));
    }

    void lateAnswerDoesNotCelebrateAgain()
    {
        Fixture f;
        const QString doneZ = R"({"goalId":"z","state":"done","time":50})";
        const QString doneA = R"({"goalId":"a","state":"done","time":100})";
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200,
                                  QStringLiteral(R"({"active":null,"last":%1})").arg(doneZ));
        f.inputs.eventReceived("goal.state", QStringLiteral(
            R"({"goalId":"a","state":"done","active":null,"last":%1})").arg(doneA));
        QTRY_COMPARE(f.clip(), QString("rest"));
        QCOMPARE(f.played.size(), 3);
        // An answer from before that event, still naming the one before.
        f.inputs.snapshotReceived(MascotInputs::goalPath(), 200,
                                  QStringLiteral(R"({"active":null,"last":%1})").arg(doneZ));
        f.inputs.eventReceived("goal.state", QStringLiteral(
            R"({"goalId":"b","state":"cancelled","active":null,"last":%1})").arg(doneA));
        QCOMPARE(f.played.size(), 3);
    }

    void confettiOwedIsThrownLater()
    {
        Fixture f;
        f.inputs.setCelebrationRetry(2000, 20);
        f.inputs.eventReceived("realtime:state", R"({"state":"listening"})");
        f.goalEvent("done", "null");
        QCOMPARE(f.clip(), QString("wait"));
        f.inputs.eventReceived("realtime:state", R"({"state":"ended"})");
        QTRY_COMPARE(f.clip(), QString("done"));
        QTRY_COMPARE(f.clip(), QString("rest"));
        QTest::qWait(100);
        QCOMPARE(f.clip(), QString("rest"));   // and only once
    }

    void confettiOwedExpires()
    {
        Fixture f;
        f.inputs.setCelebrationRetry(60, 20);
        f.inputs.eventReceived("realtime:state", R"({"state":"listening"})");
        f.goalEvent("done", "null");
        QTest::qWait(150);
        f.inputs.eventReceived("realtime:state", R"({"state":"ended"})");
        QTest::qWait(100);
        QCOMPARE(f.clip(), QString("rest"));
        QCOMPARE(f.played.size(), 3);
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

    void conversationFollowsThePhases()
    {
        Fixture f;
        auto phase = [&f](const QString &state) {
            f.inputs.eventReceived("realtime:state",
                                   QStringLiteral(R"({"type":"realtime:state","state":"%1","runId":"r"})").arg(state));
        };
        phase("connecting");
        QCOMPARE(f.played.size(), 1);   // the resting clip just carries on
        phase("listening");
        QCOMPARE(f.clip(), QString("wait"));
        QVERIFY(f.director.steadyLamp());
        phase("thinking");
        QCOMPARE(f.clip(), QString("type"));
        QVERIFY(!f.director.steadyLamp());
        phase("working");               // a tool running: still thinking
        QCOMPARE(f.clip(), QString("type"));
        phase("speaking");
        QCOMPARE(f.clip(), QString("welcome"));
        QTRY_COMPARE(f.clip(), QString("rest"));
        phase("ended");
        QCOMPARE(f.played.size(), 5);
        QTRY_VERIFY(f.asked(MascotInputs::conversationPath()) >= 1);
        const qsizetype asked = f.wanted.size();
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), asked);
    }

    void conversationSnapshot()
    {
        Fixture f;
        f.inputs.snapshotReceived(MascotInputs::conversationPath(), 200,
                                  R"({"mode":"realtime","state":"listening","connectedMs":1})");
        QCOMPARE(f.clip(), QString("wait"));
        f.inputs.snapshotReceived(MascotInputs::conversationPath(), 200, R"({"mode":"idle"})");
        QCOMPARE(f.clip(), QString("rest"));
    }

    void conversationOutranksTheGoal()
    {
        Fixture f;
        f.goalEvent("working", working("g1"));
        f.inputs.eventReceived("realtime:state", R"({"state":"listening"})");
        QCOMPARE(f.clip(), QString("wait"));
        // No confetti into the conversation.
        f.goalEvent("done", "null");
        QCOMPARE(f.clip(), QString("wait"));
        f.inputs.eventReceived("realtime:state", R"({"state":"ended"})");
        QCOMPARE(f.clip(), QString("rest"));
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
