#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

#include "../src/MascotDirector.h"

// MascotDirector as a black box: conditions, cues and stage reports go
// in, the sequence of directives comes out. Clips are 250 ms long - short,
// so that the director's own timing can be waited for, but not shorter
// than its cross-fade, below which it stops taking lengths at face value.
class TestMascotDirector : public QObject
{
    Q_OBJECT

private:
    static constexpr int clipMs = 250;

    static QJsonObject manifest()
    {
        return QJsonDocument::fromJson(R"({
            "once": ["welcome", "done", "touch.l", "touch.m1", "touch.m2", "touch.r"],
            "idle": { "base": "rest", "interludes": ["play", "nap"] },
            "work": ["type", "read", "code"],
            "conditions": { "offline": "offline", "error": "error", "waiting_user": "wait" },
            "cues": { "welcome": ["welcome"], "done": ["done"], "touch_left": ["touch.l"],
                      "touch_middle": ["touch.m1", "touch.m2"], "touch_right": ["touch.r"] }
        })").object();
    }

    static QStringList allClips()
    {
        return { "rest", "play", "nap", "type", "read", "code", "offline", "error", "wait",
                 "welcome", "done", "touch.l", "touch.m1", "touch.m2", "touch.r" };
    }

    // A director with a spy on its directives.
    struct Fixture
    {
        MascotDirector director { manifest() };
        QSignalSpy spy { &director, &MascotDirector::directive };

        Fixture()
        {
            // Always the first of the interludes on offer.
            director.setPicker([](int) { return 0; });
        }
        void attach(const QStringList &clips = allClips(), int ms = clipMs)
        {
            QVariantList durations;
            for (qsizetype i = 0; i < clips.size(); ++i)
                durations.append(ms);
            director.stageAttached(clips, durations);
        }
        // The clips asked for so far, "!" appended to those played once.
        QStringList played() const
        {
            QStringList result;
            for (const QList<QVariant> &call : spy)
                result.append(call.at(0).toString() + (call.at(1).toBool() ? "" : "!"));
            return result;
        }
    };

private slots:
    void idlesWhenNothingIsSet()
    {
        Fixture f;
        f.attach();
        QCOMPARE(f.played(), QStringList({ "rest" }));
        QCOMPARE(f.spy.first().at(2).toInt(), MascotDirector::fadeMs);
    }

    void idleAlternatesRestAndInterludes()
    {
        Fixture f;
        f.attach();
        // The picker always takes the first choice, and the interlude
        // that played last is not among the choices.
        // (The list keeps growing, so only its start can be compared.)
        QTRY_COMPARE(f.played().mid(0, 6), QStringList({ "rest", "play", "rest", "nap", "rest", "play" }));
    }

    void highestConditionWins()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.set(MascotDirector::Offline);
        f.director.set(MascotDirector::Error);       // below Offline: no change
        f.director.clear(MascotDirector::Offline);
        f.director.clear(MascotDirector::Error);
        f.director.clear(MascotDirector::Working);
        QCOMPARE(f.played(), QStringList({ "rest", "type", "offline", "error", "type", "rest" }));
    }

    void conditionsDoNotRunOnTheClock()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        QTest::qWait(clipMs * 3);
        QCOMPARE(f.played(), QStringList({ "rest", "type" }));
    }

    void goalKeepsItsWorkClip()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.set(MascotDirector::WaitingUser);
        f.director.clear(MascotDirector::WaitingUser);
        f.director.clear(MascotDirector::Working);
        f.director.set(MascotDirector::Working, "goal-1");   // same goal again
        f.director.set(MascotDirector::Working, "goal-2");   // the next one
        QCOMPARE(f.played(), QStringList({ "rest", "type", "wait", "type", "rest", "type", "read" }));
    }

    void doneEndsTheGoalAndPlaysInFull()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.clear(MascotDirector::Working);
        f.director.cue(MascotDirector::Done);
        // Arrives while the confetti is still falling: has to wait.
        f.director.set(MascotDirector::Working, "goal-1");
        QCOMPARE(f.played(), QStringList({ "rest", "type", "rest", "done!" }));
        // Then it is a new goal, even under the old name.
        QTRY_COMPARE(f.played(), QStringList({ "rest", "type", "rest", "done!", "read" }));
    }

    void noCelebratingWhileOffline()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Offline);
        f.director.cue(MascotDirector::Done);
        QCOMPARE(f.played(), QStringList({ "rest", "offline" }));
    }

    void touchesOnlyWhileIdle()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working);
        f.director.cue(MascotDirector::TouchLeft);
        QCOMPARE(f.played(), QStringList({ "rest", "type" }));
    }

    void touchPlaysInFullThenIdles()
    {
        Fixture f;
        f.attach();
        f.director.cue(MascotDirector::TouchLeft);
        f.director.cue(MascotDirector::TouchRight);   // ignored: one is playing
        QCOMPARE(f.played(), QStringList({ "rest", "touch.l!" }));
        QTRY_COMPARE(f.played().mid(0, 3), QStringList({ "rest", "touch.l!", "rest" }));
    }

    void middleTouchesTakeTurns()
    {
        Fixture f;
        f.attach();
        f.director.cue(MascotDirector::TouchMiddle);
        QTRY_COMPARE(f.played().mid(0, 3), QStringList({ "rest", "touch.m1!", "rest" }));
        f.director.cue(MascotDirector::TouchMiddle);
        QCOMPARE(f.played().last(), QString("touch.m2!"));
    }

    void conditionCutsWelcomeShort()
    {
        Fixture f;
        f.attach();
        f.director.cue(MascotDirector::Welcome);
        f.director.set(MascotDirector::Working);
        QCOMPARE(f.played(), QStringList({ "rest", "welcome!", "type" }));
    }

    void conversationPhases()
    {
        Fixture f;
        QSignalSpy lamp(&f.director, &MascotDirector::steadyLampChanged);
        f.attach();
        f.director.set(MascotDirector::Error);
        f.director.set(MascotDirector::Conversation, "preparing");
        f.director.set(MascotDirector::Conversation, "listening");
        QVERIFY(f.director.steadyLamp());
        f.director.set(MascotDirector::Conversation, "thinking");
        QVERIFY(!f.director.steadyLamp());
        f.director.set(MascotDirector::Conversation, "speaking");
        QCOMPARE(f.played(), QStringList({ "rest", "error", "rest", "wait", "type", "welcome!" }));
        // The nod once, then resting for as long as the speaking lasts.
        QTest::qWait(clipMs * 3);
        QCOMPARE(f.played().mid(6), QStringList({ "rest" }));
        // The next turn nods again.
        f.director.set(MascotDirector::Conversation, "listening");
        f.director.set(MascotDirector::Conversation, "speaking");
        QCOMPARE(f.played().mid(7), QStringList({ "wait", "welcome!" }));
        // Cut short by the end of the conversation.
        f.director.clear(MascotDirector::Conversation);
        QCOMPARE(f.played().last(), QString("error"));
        QTest::qWait(clipMs * 2);
        QCOMPARE(f.played().last(), QString("error"));
        QCOMPARE(lamp.size(), 4);
    }

    void thinkingUsesTheGoalsWorkClip()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.set(MascotDirector::Working, "goal-2");
        f.director.set(MascotDirector::Conversation, "listening");
        f.director.set(MascotDirector::Conversation, "thinking");
        f.director.clear(MascotDirector::Conversation);   // the same clip: carries on
        QCOMPARE(f.played(), QStringList({ "rest", "type", "read", "wait", "read" }));
    }

    void thinkingWithoutAGoalUsesTheNextGoalsClip()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.clear(MascotDirector::Working);
        f.director.set(MascotDirector::Conversation, "thinking");
        // The thinking turns into a goal: nothing changes.
        f.director.set(MascotDirector::Working, "goal-2");
        f.director.clear(MascotDirector::Conversation);
        QCOMPARE(f.played(), QStringList({ "rest", "type", "rest", "read" }));
    }

    void doneWhileAnotherGoalRuns()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.set(MascotDirector::Working, "goal-2");
        QVERIFY(f.director.cue(MascotDirector::Done));
        // goal-2 keeps its clip.
        QTRY_COMPARE(f.played(), QStringList({ "rest", "type", "read", "done!", "read" }));
    }

    void droppedCuesSaySo()
    {
        Fixture f;
        QVERIFY(!f.director.cue(MascotDirector::Done));   // nobody watching
        f.attach();
        QVERIFY(f.director.cue(MascotDirector::TouchLeft));
        QVERIFY(!f.director.cue(MascotDirector::Done));   // the touch plays in full
        f.director.set(MascotDirector::Offline);
        QTRY_COMPARE(f.played().last(), QString("offline"));
        QVERIFY(!f.director.cue(MascotDirector::Done));
    }

    void noTouchesInAConversation()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Conversation, "preparing");
        f.director.cue(MascotDirector::TouchLeft);
        f.director.set(MascotDirector::Working, "goal-1");
        f.director.cue(MascotDirector::Done);
        QCOMPARE(f.played(), QStringList({ "rest" }));
    }

    void steadyLampGoesWithTheStage()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Conversation, "listening");
        QVERIFY(f.director.steadyLamp());
        f.director.stageDetached();
        QVERIFY(!f.director.steadyLamp());
        f.attach();
        QVERIFY(f.director.steadyLamp());
    }

    void manualLoopsWhateverWasPicked()
    {
        Fixture f;
        f.attach();
        f.director.set(MascotDirector::Offline);
        f.director.set(MascotDirector::Manual, "done");
        f.director.set(MascotDirector::Manual, "nap");
        f.director.clear(MascotDirector::Manual);
        QCOMPARE(f.played(), QStringList({ "rest", "offline", "done", "nap", "offline" }));
    }

    void tracksConditionsWithoutAStage()
    {
        Fixture f;
        f.director.set(MascotDirector::Offline);
        f.director.cue(MascotDirector::Done);   // nobody watching: dropped
        QVERIFY(f.spy.isEmpty());
        f.attach();
        QCOMPARE(f.played(), QStringList({ "offline" }));

        f.director.stageDetached();
        f.director.clear(MascotDirector::Offline);
        QCOMPARE(f.spy.size(), 1);
        f.attach();
        QCOMPARE(f.played(), QStringList({ "offline", "rest" }));
    }

    void detachedStageStopsTheClock()
    {
        Fixture f;
        f.attach();
        f.director.stageDetached();
        QTest::qWait(clipMs * 3);
        QCOMPARE(f.played(), QStringList({ "rest" }));
    }

    void posesAreNotTimedByTheirLength()
    {
        // Clips of length 0: an idle round lasts poseRoundMs instead, a
        // reaction as long as the cross-fade.
        Fixture f;
        f.attach(allClips(), 0);
        QTest::qWait(MascotDirector::fadeMs * 2);
        QCOMPARE(f.played(), QStringList({ "rest" }));
        f.director.cue(MascotDirector::TouchLeft);
        QTRY_COMPARE(f.played(), QStringList({ "rest", "touch.l!", "rest" }));
    }

    void modelWithOtherClips()
    {
        // Nothing the manifest names: the first clip is the resting one,
        // it just loops, conditions fall back on it (without restarting
        // it) and cues do nothing.
        Fixture f;
        f.attach({ "walk", "run" });
        f.director.set(MascotDirector::Offline);
        f.director.clear(MascotDirector::Offline);
        f.director.cue(MascotDirector::TouchLeft);
        f.director.set(MascotDirector::Manual, "run");
        QTest::qWait(clipMs * 3);
        QCOMPARE(f.played(), QStringList({ "walk", "run" }));
    }

    void modelWithoutClips()
    {
        Fixture f;
        f.attach({});
        f.director.set(MascotDirector::Offline);
        f.director.cue(MascotDirector::Welcome);
        QVERIFY(f.spy.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestMascotDirector)
#include "test_mascot_director.moc"
