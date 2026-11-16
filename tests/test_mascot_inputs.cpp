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
            "idle": { "base": "rest" },
            "conditions": { "offline": "offline", "waiting_user": "wait" }
        })").object() };
        MascotInputs inputs { &director, pollMs };
        QSignalSpy played { &director, &MascotDirector::directive };
        QSignalSpy wanted { &inputs, &MascotInputs::authorizationStatusWanted };

        Fixture()
        {
            director.stageAttached({ "rest", "offline", "wait" }, { 250, 250, 250 });
        }
        QString clip() const { return played.last().at(0).toString(); }
        void authorizationEvent(const QString &state, const QString &reason = QString())
        {
            inputs.eventReceived("notification", QStringLiteral(
                R"({"type":"notification","kind":"authorization","state":"%1","notifId":"c1","data":{"reason":"%2"}})")
                .arg(state, reason));
        }
    };

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
        QCOMPARE(f.wanted.size(), 1);
        f.inputs.authorizationStatusReceived(200, pending());
        QCOMPARE(f.clip(), QString("wait"));
    }

    void openEventShowsAtOnceAndIsChecked()
    {
        Fixture f;
        f.authorizationEvent("open");
        QCOMPARE(f.clip(), QString("wait"));
        QCOMPARE(f.wanted.size(), 1);
        // The snapshot naming the request differently restarts nothing.
        f.inputs.authorizationStatusReceived(200, pending());
        QCOMPARE(f.played.size(), 2);
    }

    void resolvedEventLeavesItToTheSnapshot()
    {
        Fixture f;
        f.authorizationEvent("open");
        // The button is no longer armed, the request still waits.
        f.authorizationEvent("resolved", "paused");
        f.inputs.authorizationStatusReceived(200, pending());
        QCOMPARE(f.clip(), QString("wait"));
        f.authorizationEvent("resolved", "satisfied");
        f.inputs.authorizationStatusReceived(200, none());
        QCOMPARE(f.clip(), QString("rest"));
    }

    void pollsOnlyWhileWaiting()
    {
        Fixture f;
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), 0);
        f.inputs.authorizationStatusReceived(200, pending());
        QTRY_VERIFY(f.wanted.size() >= 2);
        // Resolved elsewhere, and no event came: the poll finds out.
        f.inputs.authorizationStatusReceived(200, none());
        QCOMPARE(f.clip(), QString("rest"));
        const qsizetype asked = f.wanted.size();
        QTest::qWait(pollMs * 3);
        QCOMPARE(f.wanted.size(), asked);
    }

    void edgeGoneEndsTheWait()
    {
        Fixture f;
        f.inputs.authorizationStatusReceived(200, pending());
        f.inputs.authorizationStatusReceived(0, QString());
        QCOMPARE(f.clip(), QString("rest"));
        f.inputs.authorizationStatusReceived(200, pending());
        f.inputs.authorizationStatusReceived(503, R"({"error":"auth broker not initialized"})");
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

    void offlineOutranksWaiting()
    {
        Fixture f;
        f.inputs.authorizationStatusReceived(200, pending());
        f.inputs.connectivityProbed(false);
        f.inputs.connectivityProbed(false);
        QCOMPARE(f.clip(), QString("offline"));
        f.inputs.connectivityProbed(true);
        QCOMPARE(f.clip(), QString("wait"));
    }
};

QTEST_GUILESS_MAIN(TestMascotInputs)
#include "test_mascot_inputs.moc"
