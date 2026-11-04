#include "StatusStore.h"
#include "RestClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QString kWifiPath = QStringLiteral("/status/wifi");
const QString kConnectivityPath = QStringLiteral("/status/connectivity");
const QString kHdmiPath = QStringLiteral("/status/hdmi");
const QString kHardwarePath = QStringLiteral("/status/hardware");
const QString kSystemPath = QStringLiteral("/status/system");
}

StatusStore::StatusStore(RestClient *rest, QObject *parent)
    : QObject(parent)
    , m_rest(rest)
{
    connect(m_rest, &RestClient::requestSucceeded, this,
            [this](const QString &path, int statusCode, const QString &body, qint64, const QVariantMap &) {
                if (path == kWifiPath || path == kConnectivityPath || path == kHdmiPath
                    || path == kHardwarePath || path == kSystemPath)
                    onSucceeded(path, statusCode, body);
            });
    connect(m_rest, &RestClient::requestFailed, this,
            [this](const QString &path, const QString &errorString) {
                if (path == kWifiPath || path == kConnectivityPath || path == kHdmiPath
                    || path == kHardwarePath || path == kSystemPath)
                    onFailed(path, errorString);
            });

    m_pollTimer.setInterval(5000);
    connect(&m_pollTimer, &QTimer::timeout, this, &StatusStore::refreshAll);
}

void StatusStore::start()
{
    if (m_started)
        return;
    m_started = true;
    refreshAll();
    m_pollTimer.start();
}

void StatusStore::refreshAll()
{
    m_rest->get(kWifiPath);
    m_rest->get(kConnectivityPath);
    m_rest->get(kHdmiPath);
    m_rest->get(kHardwarePath);
    m_rest->get(kSystemPath);
}

bool StatusStore::ready() const
{
    return m_wifiKnown && m_connectivityKnown && m_hdmiKnown && m_hardwareKnown && m_systemKnown;
}

void StatusStore::onSucceeded(const QString &path, int statusCode, const QString &body)
{
    if (statusCode < 200 || statusCode >= 300)
        return;

    const QJsonObject obj = QJsonDocument::fromJson(body.toUtf8()).object();
    const bool wasReady = ready();

    if (path == kWifiPath) {
        m_wifiKnown = true;
        m_wifiConnected = obj.value("connected").toBool();
        m_wifiSignal = obj.value("signal").toInt();
        emit wifiChanged();
    } else if (path == kConnectivityPath) {
        m_connectivityKnown = true;
        m_connectivityOk = obj.value("ok").toBool();
        m_connectivityLatencyMs = obj.value("latencyMs").toInt();
        emit connectivityChanged();
    } else if (path == kHdmiPath) {
        m_hdmiKnown = true;
        m_hdmiConnected = obj.value("connected").toBool();
        QVariantList inputs;
        for (const QJsonValue &v : obj.value("inputs").toArray()) {
            const QJsonObject i = v.toObject();
            QVariantMap m;
            m["id"] = i.value("id").toString();
            m["label"] = i.value("label").toString();
            m["primary"] = i.value("primary").toBool();
            m["connected"] = i.value("connected").toBool();
            inputs.append(m);
        }
        m_hdmiInputs = inputs;
        emit hdmiChanged();
    } else if (path == kHardwarePath) {
        m_hardwareKnown = true;
        m_usbHostConnected = obj.value("usbHost").toObject().value("connected").toBool();
        m_pairingSerialOpen = obj.value("pairingSerial").toObject().value("open").toBool();
        emit hardwareChanged();
    } else if (path == kSystemPath) {
        m_systemKnown = true;
        m_boardProfile = obj.value("boardProfile").toString();
        m_deviceSn = obj.value("sn").toString();
        emit systemChanged();
    }

    if (!wasReady && ready())
        emit readyChanged();
}

void StatusStore::onFailed(const QString &path, const QString &errorString)
{
    // Leave the last-known value in place (matches edge-front2: a
    // transient poll failure shows the last good state, not an immediate
    // "unknown" flicker) - just log it for now via qWarning through Qt's
    // default message handler (no dedicated logger in this app yet).
    qWarning("StatusStore: %s failed: %s", qPrintable(path), qPrintable(errorString));
}
