#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>

// Minimal REST helper for validation item C in docs/findings.md: confirm
// Qt's QNetworkAccessManager can GET/POST JSON against the real
// `violoop-edge` service (../../device, GET/POST http://127.0.0.1:9090)
// edge-front2 already talks to over Dio. No auth/headers needed - the
// protocol is a loopback-only trust boundary (see
// ../../device/docs/protocols/local-http.md).
class RestClient : public QObject
{
    Q_OBJECT
public:
    explicit RestClient(QObject *parent = nullptr);

    // path is e.g. "/status/service". jsonBody for postJson is sent as-is
    // with Content-Type: application/json (caller is responsible for
    // producing valid JSON - this is a spike, not a hardened client).
    Q_INVOKABLE void get(const QString &path);
    Q_INVOKABLE void postJson(const QString &path, const QString &jsonBody);

signals:
    void requestSucceeded(const QString &path, int statusCode, const QString &body);
    void requestFailed(const QString &path, const QString &errorString);

private:
    void send(const QString &path, const QByteArray &method, const QByteArray &body);

    QNetworkAccessManager m_nam;
    QUrl m_base;
};
