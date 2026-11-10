#include "TimelineLoopCloser.h"

#include <QDebug>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlListReference>
#include <QUrl>
#include <QVariant>

TimelineLoopCloser::TimelineLoopCloser(QObject *parent)
    : QObject(parent)
{
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
