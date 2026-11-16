#include "UpgradeStore.h"
#include "RestClient.h"
#include "SseClient.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QString kVersionsPath = QStringLiteral("/upgrade/versions");
const QString kStatusPath = QStringLiteral("/upgrade/status");
const QString kTriggerPath = QStringLiteral("/upgrade/trigger");

// How long a non-terminal progress record stays believable when it rests
// only on a run file the poll handed us (not a live push) - matches the
// device's own freshness bound for replaying an interrupted run.
constexpr qint64 kInFlightMaxAgeMs = 10 * 60 * 1000;
// How long the device may stay unreachable while still read as "being
// upgraded" rather than broken - the blackout an OTA causes is one edge
// restart (~93s measured on dev2), well past that silence is a fault.
constexpr qint64 kUnreachableGraceMs = 5 * 60 * 1000;
// How long progress may sit still before WE call it wedged, independent
// of the device's own `stalled` bit (a run that dies mid-download stops
// producing events, and "no events" looks identical to a healthy slow one).
constexpr qint64 kStallThresholdMs = 90 * 1000;
constexpr int kIdleIntervalMs = 5000;
constexpr int kBusyIntervalMs = 2000;

bool progressTerminal(const QVariantMap &p)
{
    if (p.contains("terminal"))
        return p.value("terminal").toBool();
    const QString phase = p.value("phase").toString();
    return phase == "done" || phase == "failed";
}

QString progressKeyOf(const QVariantMap &p)
{
    if (p.isEmpty())
        return QString();
    return QStringLiteral("%1|%2|%3|%4|%5")
        .arg(p.value("runId").toString())
        .arg(p.value("attempt").toString())
        .arg(p.value("phase").toString())
        .arg(p.value("percent").toString())
        .arg(p.value("updatedAtMs").toString());
}
}

UpgradeStore::UpgradeStore(RestClient *rest, SseClient *sse, QObject *parent)
    : QObject(parent)
    , m_rest(rest)
    , m_sse(sse)
{
    connect(m_rest, &RestClient::requestSucceeded, this,
            [this](const QString &path, int statusCode, const QString &body, qint64, const QVariantMap &) {
                if (path == kVersionsPath || path == kStatusPath || path == kTriggerPath)
                    onRestSucceeded(path, statusCode, body);
            });
    connect(m_rest, &RestClient::requestFailed, this,
            [this](const QString &path, const QString &errorString) {
                if (path == kVersionsPath || path == kStatusPath || path == kTriggerPath)
                    onRestFailed(path, errorString);
            });

    connect(m_sse, &SseClient::eventReceived, this,
            [this](const QString &type, const QString &data) {
                if (type != "upgrade:progress")
                    return;
                const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
                // Protocol doc describes this as "nested progress object,
                // same shape as GET /upgrade/status" - defensively accept
                // either a top-level {"progress": {...}} envelope or the
                // progress fields directly on the event payload, and
                // confirm which on real hardware (see docs/progress.md).
                const QJsonObject progressObj = obj.contains("progress")
                    ? obj.value("progress").toObject()
                    : obj;
                applyProgress(progressObj.toVariantMap());
            });

    // The push has a blind spot exactly where it matters (edge restarting
    // mid-OTA); after a reconnect, go read the truth rather than wait for
    // a frame that may never describe this run.
    connect(m_sse, &SseClient::connected, this, [this]() {
        if (m_started)
            refreshStatus();
    });

    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &UpgradeStore::refreshStatus);
}

void UpgradeStore::start()
{
    m_started = true;
    refreshAll();
    retune(/*force=*/true);
}

void UpgradeStore::stop()
{
    m_started = false;
    m_timer.stop();
    m_currentIntervalMs = -1;
}

void UpgradeStore::refreshAll()
{
    refreshVersions();
    refreshStatus();
}

void UpgradeStore::refreshVersions()
{
    m_rest->get(kVersionsPath);
}

void UpgradeStore::refreshStatus()
{
    m_rest->get(kStatusPath);
}

void UpgradeStore::trigger()
{
    if (m_triggering)
        return;
    m_triggering = true;
    m_triggerError.clear();
    emit triggerChanged();
    m_rest->postJson(kTriggerPath, QStringLiteral("{}"));
}

void UpgradeStore::onRestSucceeded(const QString &path, int statusCode, const QString &body)
{
    const QJsonObject obj = QJsonDocument::fromJson(body.toUtf8()).object();
    const bool ok = statusCode >= 200 && statusCode < 300;

    if (path == kVersionsPath) {
        if (ok) {
            m_versions = obj.toVariantMap();
            m_versionsError.clear();
        } else {
            m_versionsError = QStringLiteral("HTTP %1").arg(statusCode);
        }
        emit versionsChanged();
        return;
    }

    if (path == kStatusPath) {
        if (!ok) {
            onRestFailed(path, QStringLiteral("HTTP %1").arg(statusCode));
            return;
        }
        const bool wasInFlight = m_inFlight;
        m_status = obj.toVariantMap();
        m_statusError.clear();
        m_unreachable = false;
        m_unreachableSinceMs = -1;

        const QVariantMap polledProgress = m_status.value("progress").toMap();
        if (!m_pushed.isEmpty() && polledCaughtUp(polledProgress))
            m_pushed.clear();

        noteProgressMovement();
        m_inFlight = isInFlight();
        if (wasInFlight && !m_inFlight)
            refreshVersions();
        retune();

        emit statusChanged();
        emit progressChanged();
        emit stateChanged();
        return;
    }

    if (path == kTriggerPath) {
        m_inFlight = true;
        m_triggering = false;
        retune();
        emit triggerChanged();
        emit stateChanged();
        refreshStatus();
        return;
    }
}

void UpgradeStore::onRestFailed(const QString &path, const QString &errorString)
{
    if (path == kVersionsPath) {
        m_versionsError = errorString;
        emit versionsChanged();
        return;
    }

    if (path == kStatusPath) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_unreachableSinceMs < 0)
            m_unreachableSinceMs = now;
        if (m_inFlight && (now - m_unreachableSinceMs) <= kUnreachableGraceMs) {
            // Expected mid-upgrade: edge is being replaced under us.
            m_unreachable = true;
            m_statusError.clear();
        } else {
            m_unreachable = false;
            m_statusError = errorString;
        }
        emit statusChanged();
        emit stateChanged();
        return;
    }

    if (path == kTriggerPath) {
        m_triggerError = errorString;
        m_triggering = false;
        emit triggerChanged();
        return;
    }
}

void UpgradeStore::applyProgress(const QVariantMap &progress)
{
    m_pushed = progress;
    noteProgressMovement();
    if (!progressTerminal(progress)) {
        m_inFlight = true;
        m_unreachable = false;
    } else if (m_started) {
        // The push says this round is over; only activeState can confirm
        // the unit actually exited, so go ask instead of declaring it done.
        refreshStatus();
    }
    retune();
    emit progressChanged();
    emit stateChanged();
}

UpgradeStore::MergeResult UpgradeStore::mergedProgress() const
{
    const QVariantMap polled = m_status.value("progress").toMap();
    const QVariantMap &pushed = m_pushed;

    if (pushed.isEmpty() && polled.isEmpty())
        return {};
    if (pushed.isEmpty())
        return {polled, false, true};
    if (polled.isEmpty())
        return {pushed, true, true};

    bool pushedOk = false, polledOk = false;
    const qint64 pushedAt = pushed.value("updatedAtMs").toLongLong(&pushedOk);
    const qint64 polledAt = polled.value("updatedAtMs").toLongLong(&polledOk);
    if (!pushedOk || !polledOk)
        return {polled, false, true};
    return pushedAt > polledAt ? MergeResult{pushed, true, true} : MergeResult{polled, false, true};
}

QVariantMap UpgradeStore::progress() const
{
    return mergedProgress().map;
}

bool UpgradeStore::busy() const
{
    const QString state = m_status.value("activeState").toString();
    return state == "active" || state == "activating";
}

bool UpgradeStore::isInFlight() const
{
    if (busy())
        return true;
    const MergeResult p = mergedProgress();
    if (!p.valid || progressTerminal(p.map))
        return false;
    const qint64 bound = p.fromPushed ? kStallThresholdMs : kInFlightMaxAgeMs;
    return qint64(progressAgeSeconds()) * 1000 < bound;
}

bool UpgradeStore::polledCaughtUp(const QVariantMap &polled) const
{
    if (polled.isEmpty())
        return false;
    bool pushedOk = false, polledOk = false;
    const qint64 pushedAt = m_pushed.value("updatedAtMs").toLongLong(&pushedOk);
    const qint64 polledAt = polled.value("updatedAtMs").toLongLong(&polledOk);
    if (!pushedOk || !polledOk)
        return true;
    return polledAt >= pushedAt;
}

void UpgradeStore::noteProgressMovement()
{
    const QVariantMap p = mergedProgress().map;
    const QString key = progressKeyOf(p);
    if (key == m_progressKey)
        return;
    m_progressKey = key;
    m_progressMovedAtMs = QDateTime::currentMSecsSinceEpoch();
}

bool UpgradeStore::stalled() const
{
    if (m_status.value("stalled").toBool())
        return true;
    const MergeResult p = mergedProgress();
    if (!upgrading() || m_progressMovedAtMs < 0 || !p.valid || progressTerminal(p.map))
        return false;
    return (QDateTime::currentMSecsSinceEpoch() - m_progressMovedAtMs) > kStallThresholdMs;
}

int UpgradeStore::progressAgeSeconds() const
{
    bool ok = false;
    qint64 deviceAgeMs = mergedProgress().map.value("ageMs").toLongLong(&ok);
    if (!ok)
        deviceAgeMs = 0;
    qint64 observedMs = 0;
    if (m_progressMovedAtMs >= 0)
        observedMs = QDateTime::currentMSecsSinceEpoch() - m_progressMovedAtMs;
    return int((deviceAgeMs > observedMs ? deviceAgeMs : observedMs) / 1000);
}

void UpgradeStore::retune(bool force)
{
    if (!m_started)
        return;
    const int wanted = upgrading() ? kBusyIntervalMs : kIdleIntervalMs;
    if (!force && wanted == m_currentIntervalMs)
        return;
    m_timer.stop();
    m_currentIntervalMs = wanted;
    m_timer.setInterval(wanted);
    m_timer.start();
}
