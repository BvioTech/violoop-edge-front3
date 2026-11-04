#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantMap>

class RestClient;
class SseClient;

// Port of edge-front2's lib/stores/upgrade_store.dart. This is the most
// business-logic-heavy piece in the whole app - see that file's own
// comments (kept verbatim below where the C++ mirrors them) for *why* each
// rule exists. Data is kept as QVariantMap (parsed JSON) rather than typed
// C++ structs for `versions`/`status`/`progress`: QML reads these fields
// generically (progress.phase, progress.percent, ...), and the real
// business logic that needs type safety (the in-flight/stalled/merge
// rules below) only ever touches a handful of named fields, which get
// explicit helpers rather than a full model class.
class UpgradeStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap versions READ versions NOTIFY versionsChanged)
    Q_PROPERTY(QString versionsError READ versionsError NOTIFY versionsChanged)
    Q_PROPERTY(QVariantMap status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusError READ statusError NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
    Q_PROPERTY(bool upgrading READ upgrading NOTIFY stateChanged)
    Q_PROPERTY(bool unreachable READ unreachable NOTIFY stateChanged)
    Q_PROPERTY(bool stalled READ stalled NOTIFY stateChanged)
    Q_PROPERTY(int progressAgeSeconds READ progressAgeSeconds NOTIFY stateChanged)
    Q_PROPERTY(bool triggering READ triggering NOTIFY triggerChanged)
    Q_PROPERTY(QString triggerError READ triggerError NOTIFY triggerChanged)

public:
    explicit UpgradeStore(RestClient *rest, SseClient *sse, QObject *parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void trigger();
    Q_INVOKABLE void refreshAll();

    QVariantMap versions() const { return m_versions; }
    QString versionsError() const { return m_versionsError; }
    QVariantMap status() const { return m_status; }
    QString statusError() const { return m_statusError; }
    // The freshest progress we have, push or poll - ties and unknowns go
    // to the polled copy (it carries the authoritative activeState).
    QVariantMap progress() const;
    bool busy() const;
    bool upgrading() const { return busy() || m_inFlight; }
    bool unreachable() const { return m_unreachable; }
    bool stalled() const;
    int progressAgeSeconds() const;
    bool triggering() const { return m_triggering; }
    QString triggerError() const { return m_triggerError; }

signals:
    void versionsChanged();
    void statusChanged();
    void progressChanged();
    void stateChanged();
    void triggerChanged();

private:
    struct MergeResult
    {
        QVariantMap map;
        bool fromPushed = false;
        bool valid = false;
    };

    void refreshVersions();
    void refreshStatus();
    void applyProgress(const QVariantMap &progress);
    void onRestSucceeded(const QString &path, int statusCode, const QString &body);
    void onRestFailed(const QString &path, const QString &errorString);

    MergeResult mergedProgress() const;
    bool isInFlight() const;
    bool polledCaughtUp(const QVariantMap &polled) const;
    void noteProgressMovement();
    void retune(bool force = false);

    RestClient *m_rest;
    SseClient *m_sse;
    QTimer m_timer;
    bool m_started = false;
    int m_currentIntervalMs = -1;

    QVariantMap m_versions;
    QString m_versionsError;
    QVariantMap m_status; // raw /upgrade/status object, including nested "progress"
    QString m_statusError;
    QVariantMap m_pushed; // last upgrade:progress SSE frame, empty = none
    bool m_unreachable = false;
    qint64 m_unreachableSinceMs = -1;
    bool m_inFlight = false;
    bool m_triggering = false;
    QString m_triggerError;

    qint64 m_progressMovedAtMs = -1;
    QString m_progressKey;
};
