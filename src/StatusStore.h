#pragma once

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>

class RestClient;

// Equivalent of edge-front2's lib/stores/status_store.dart, scoped to what
// the home page's top-left icon cluster and the factory-test board-profile
// gate actually need (see ../../edge-front2/lib/shared/widgets/app_scaffold.dart
// `_StatusIcons` and lib/stores/status_store.dart). Pure 5-second polling,
// no SSE - edge-front2 confirms there's no push event for these fields.
//
// Real response shapes (dev1, 2026-11-04, see docs/progress.md):
//   GET /status/wifi         {"connected":bool,"signal":0-100,...}
//   GET /status/connectivity {"ok":bool,"latencyMs":int}
//   GET /status/hdmi         {"connected":bool,"inputs":[{"id","label","primary","connected"},...]}
//   GET /status/hardware     {"usbHost":{"connected":bool},"pairingSerial":{"open":bool},...}
//   GET /status/system       {..."boardProfile":"VLA11"|...}
class StatusStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool wifiKnown READ wifiKnown NOTIFY wifiChanged)
    Q_PROPERTY(bool wifiConnected READ wifiConnected NOTIFY wifiChanged)
    Q_PROPERTY(int wifiSignal READ wifiSignal NOTIFY wifiChanged)

    Q_PROPERTY(bool connectivityKnown READ connectivityKnown NOTIFY connectivityChanged)
    Q_PROPERTY(bool connectivityOk READ connectivityOk NOTIFY connectivityChanged)
    Q_PROPERTY(int connectivityLatencyMs READ connectivityLatencyMs NOTIFY connectivityChanged)

    Q_PROPERTY(bool hdmiKnown READ hdmiKnown NOTIFY hdmiChanged)
    Q_PROPERTY(bool hdmiConnected READ hdmiConnected NOTIFY hdmiChanged)
    Q_PROPERTY(QVariantList hdmiInputs READ hdmiInputs NOTIFY hdmiChanged)

    Q_PROPERTY(bool hardwareKnown READ hardwareKnown NOTIFY hardwareChanged)
    Q_PROPERTY(bool usbHostConnected READ usbHostConnected NOTIFY hardwareChanged)
    Q_PROPERTY(bool pairingSerialOpen READ pairingSerialOpen NOTIFY hardwareChanged)

    Q_PROPERTY(QString boardProfile READ boardProfile NOTIFY systemChanged)
    Q_PROPERTY(QString deviceSn READ deviceSn NOTIFY systemChanged)

    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit StatusStore(RestClient *rest, QObject *parent = nullptr);

    // Idempotent, same contract as edge-front2's StatusStore.start() -
    // safe to call from every page that wants the status header live.
    Q_INVOKABLE void start();

    bool wifiKnown() const { return m_wifiKnown; }
    bool wifiConnected() const { return m_wifiConnected; }
    int wifiSignal() const { return m_wifiSignal; }

    bool connectivityKnown() const { return m_connectivityKnown; }
    bool connectivityOk() const { return m_connectivityOk; }
    int connectivityLatencyMs() const { return m_connectivityLatencyMs; }

    bool hdmiKnown() const { return m_hdmiKnown; }
    bool hdmiConnected() const { return m_hdmiConnected; }
    QVariantList hdmiInputs() const { return m_hdmiInputs; }

    bool hardwareKnown() const { return m_hardwareKnown; }
    bool usbHostConnected() const { return m_usbHostConnected; }
    bool pairingSerialOpen() const { return m_pairingSerialOpen; }

    QString boardProfile() const { return m_boardProfile; }
    QString deviceSn() const { return m_deviceSn; }

    bool ready() const;

signals:
    void wifiChanged();
    void connectivityChanged();
    void hdmiChanged();
    void hardwareChanged();
    void systemChanged();
    void readyChanged();

private:
    void refreshAll();
    void onSucceeded(const QString &path, int statusCode, const QString &body);
    void onFailed(const QString &path, const QString &errorString);
    void noteRecovered(const QString &path);

    RestClient *m_rest;
    QTimer m_pollTimer;
    bool m_started = false;
    // Paths whose last poll failed - see onFailed().
    QSet<QString> m_failingPaths;

    bool m_wifiKnown = false;
    bool m_wifiConnected = false;
    int m_wifiSignal = 0;

    bool m_connectivityKnown = false;
    bool m_connectivityOk = false;
    int m_connectivityLatencyMs = 0;

    bool m_hdmiKnown = false;
    bool m_hdmiConnected = false;
    QVariantList m_hdmiInputs;

    bool m_hardwareKnown = false;
    bool m_usbHostConnected = false;
    bool m_pairingSerialOpen = false;

    QString m_boardProfile;
    QString m_deviceSn;
    bool m_systemKnown = false;
};
