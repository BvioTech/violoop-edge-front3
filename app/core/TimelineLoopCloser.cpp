#include "TimelineLoopCloser.h"

#include <QDebug>
#include <QList>
#include <QPair>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QSet>
#include <QUrl>
#include <QVariant>
#include <QVector3D>

#include <algorithm>

namespace {

// One glTF clip as RuntimeLoader generates it.
struct Clip
{
    QObject *timeline = nullptr;
    QObject *animation = nullptr; // may be null if the importer's layout changes
    QString name;
};

QList<Clip> findClips(QObject *sceneRoot)
{
    QList<Clip> clips;
    if (!sceneRoot)
        return clips;
    const auto objects = sceneRoot->findChildren<QObject *>();
    for (QObject *object : objects) {
        if (!object->inherits("QQuickTimeline"))
            continue;
        Clip clip;
        clip.timeline = object;
        // Same two ways of reaching the animation as setPaused().
        const auto children = object->findChildren<QObject *>();
        for (QObject *child : children) {
            if (child->inherits("QQuickTimelineAnimation")) {
                clip.animation = child;
                break;
            }
        }
        if (!clip.animation) {
            QQmlListReference list(object, "animations");
            if (list.isValid() && list.count() > 0)
                clip.animation = list.at(0);
        }
        clip.name = clip.animation ? clip.animation->objectName() : QString();
        if (clip.name.isEmpty())
            clip.name = QStringLiteral("clip %1").arg(clips.size());
        clips.append(clip);
    }
    return clips;
}

// Same threshold as AwayPage.qml's hideCollapsedNodes(): a node scaled
// down this far is the model's way of saying "hidden".
bool isCollapsed(const QVariant &scale)
{
    const QVector3D s = scale.value<QVector3D>();
    return qMax(qAbs(s.x()), qMax(qAbs(s.y()), qAbs(s.z()))) < 0.001f;
}

// How long (in timeline frames = ms) a hidden<->shown switch may take: far
// below one display frame, so no rendered frame lands inside it.
const qreal switchFrames = 0.01;

} // namespace

TimelineLoopCloser::TimelineLoopCloser(QObject *parent)
    : QObject(parent)
{
}

QStringList TimelineLoopCloser::clipNames(QObject *sceneRoot)
{
    QStringList names;
    const auto clips = findClips(sceneRoot);
    for (const Clip &clip : clips)
        names.append(clip.name);
    return names;
}

bool TimelineLoopCloser::playClip(QObject *sceneRoot, const QString &name)
{
    const auto clips = findClips(sceneRoot);
    const Clip *wanted = nullptr;
    for (const Clip &clip : clips) {
        if (clip.name == name) {
            wanted = &clip;
            break;
        }
    }
    if (!wanted) {
        qWarning() << "TimelineLoopCloser: no clip named" << name;
        return false;
    }

    // Old clip(s) off first - see the header for why the order matters.
    for (const Clip &clip : clips) {
        if (&clip == wanted)
            continue;
        if (clip.animation)
            clip.animation->setProperty("running", false);
        clip.timeline->setProperty("enabled", false);
    }
    wanted->timeline->setProperty("enabled", true);
    if (wanted->animation)
        QMetaObject::invokeMethod(wanted->animation, "restart");
    return true;
}

int TimelineLoopCloser::restoreHolds(QObject *sceneRoot)
{
    if (!sceneRoot)
        return 0;
    QQmlEngine *engine = qmlEngine(sceneRoot);
    if (!engine) {
        qWarning() << "TimelineLoopCloser: no QML engine for" << sceneRoot;
        return 0;
    }
    QQmlComponent keyframeComponent(engine);
    keyframeComponent.setData("import QtQuick.Timeline\nKeyframe {}", QUrl());

    struct Key
    {
        qreal frame;
        QObject *object;
    };

    int inserted = 0;
    const auto clips = findClips(sceneRoot);
    for (const Clip &clip : clips) {
        // Pass 1: every group's keys in frame order, and the smallest gap
        // between neighbours anywhere in the clip (= its sampling interval).
        QList<QPair<QObject *, QList<Key>>> groups;
        qreal interval = 0;
        const auto children = clip.timeline->findChildren<QObject *>();
        for (QObject *group : children) {
            if (!group->inherits("QQuickKeyframeGroup"))
                continue;
            QQmlListReference keyframes(group, "keyframes");
            if (!keyframes.isValid() || !keyframes.canAppend())
                continue;
            QList<Key> keys;
            for (qsizetype i = 0; i < keyframes.count(); ++i)
                keys.append({ keyframes.at(i)->property("frame").toReal(), keyframes.at(i) });
            std::sort(keys.begin(), keys.end(), [](const Key &a, const Key &b) { return a.frame < b.frame; });
            for (qsizetype i = 1; i < keys.size(); ++i) {
                const qreal gap = keys[i].frame - keys[i - 1].frame;
                if (gap > 0 && (interval == 0 || gap < interval))
                    interval = gap;
            }
            groups.append({ group, keys });
        }
        if (interval <= 0)
            continue;

        // Pass 2: a gap of two intervals or more means keys were dropped
        // there, i.e. the value held (1.5 rather than 2 to tolerate the
        // float jitter in real key times).
        for (const auto &entry : std::as_const(groups)) {
            QQmlListReference keyframes(entry.first, "keyframes");
            const QList<Key> &keys = entry.second;
            const bool isScale = entry.first->property("property").toString() == QLatin1String("scale");
            for (qsizetype i = 1; i < keys.size(); ++i) {
                // A scale segment with one end collapsed is the model
                // switching something on or off, not a motion - see the
                // header. It must not be interpolated at all, whether or
                // not keys were dropped in it.
                const bool isSwitch = isScale
                    && isCollapsed(keys[i - 1].object->property("value"))
                           != isCollapsed(keys[i].object->property("value"));
                const qreal gap = keys[i].frame - keys[i - 1].frame;
                if (!isSwitch && gap < interval * 1.5)
                    continue;
                const qreal holdFor = isSwitch ? gap - qMin(gap / 2, switchFrames) : gap - interval;
                QObject *keyframe = keyframeComponent.create();
                if (!keyframe) {
                    qWarning() << "TimelineLoopCloser: cannot create Keyframe:"
                               << keyframeComponent.errorString();
                    return inserted;
                }
                keyframe->setParent(entry.first);
                keyframe->setProperty("frame", keys[i - 1].frame + holdFor);
                keyframe->setProperty("value", keys[i - 1].object->property("value"));
                keyframes.append(keyframe);
                ++inserted;
            }
        }
    }

    qInfo() << "TimelineLoopCloser: restored" << inserted << "held keyframe(s) in" << clips.size() << "clip(s)";
    return inserted;
}

int TimelineLoopCloser::closeLoops(QObject *sceneRoot)
{
    if (!sceneRoot)
        return 0;
    QQmlEngine *engine = qmlEngine(sceneRoot);
    if (!engine) {
        qWarning() << "TimelineLoopCloser: no QML engine for" << sceneRoot;
        return 0;
    }

    // QQuickKeyframe has no public C++ header; instantiate it by its QML
    // name instead.
    QQmlComponent keyframeComponent(engine);
    keyframeComponent.setData("import QtQuick.Timeline\nKeyframe {}", QUrl());

    int patched = 0;
    const auto objects = sceneRoot->findChildren<QObject *>();
    for (QObject *timeline : objects) {
        if (!timeline->inherits("QQuickTimeline"))
            continue;
        const qreal startFrame = timeline->property("startFrame").toReal();

        const auto groups = timeline->findChildren<QObject *>();
        for (QObject *group : groups) {
            if (!group->inherits("QQuickKeyframeGroup"))
                continue;
            QQmlListReference keyframes(group, "keyframes");
            if (!keyframes.isValid() || !keyframes.canAppend() || keyframes.count() == 0)
                continue;

            QObject *first = nullptr;
            QObject *last = nullptr;
            for (qsizetype i = 0; i < keyframes.count(); ++i) {
                QObject *keyframe = keyframes.at(i);
                const qreal frame = keyframe->property("frame").toReal();
                if (!first || frame < first->property("frame").toReal())
                    first = keyframe;
                if (!last || frame > last->property("frame").toReal())
                    last = keyframe;
            }
            if (first->property("frame").toReal() <= startFrame)
                continue; // already keyed at the loop start

            QObject *keyframe = keyframeComponent.create();
            if (!keyframe) {
                qWarning() << "TimelineLoopCloser: cannot create Keyframe:"
                           << keyframeComponent.errorString();
                return patched;
            }
            keyframe->setParent(group);
            keyframe->setProperty("frame", startFrame);
            keyframe->setProperty("value", last->property("value"));
            keyframes.append(keyframe);
            ++patched;
        }
    }

    qInfo() << "TimelineLoopCloser: closed the loop on" << patched << "keyframe group(s)";
    return patched;
}

int TimelineLoopCloser::setPaused(QObject *sceneRoot, bool paused)
{
    if (!sceneRoot)
        return 0;

    // The generated QQuickTimelineAnimation is expected to be a QObject
    // child of its Timeline, but also walk each Timeline's `animations`
    // list in case it isn't parented that way.
    QSet<QObject *> animations;
    const auto objects = sceneRoot->findChildren<QObject *>();
    for (QObject *object : objects) {
        if (object->inherits("QQuickTimelineAnimation")) {
            animations.insert(object);
        } else if (object->inherits("QQuickTimeline")) {
            QQmlListReference list(object, "animations");
            for (qsizetype i = 0; list.isValid() && i < list.count(); ++i) {
                if (QObject *animation = list.at(i))
                    animations.insert(animation);
            }
        }
    }

    // Only the clip that is playing: playClip() stops the others, and Qt
    // warns about pausing an animation that isn't running.
    for (QObject *animation : std::as_const(animations)) {
        if (animation->property("running").toBool())
            animation->setProperty("paused", paused);
    }

    if (animations.isEmpty())
        qWarning() << "TimelineLoopCloser: no timeline animation found to" << (paused ? "pause" : "resume");
    return animations.size();
}
