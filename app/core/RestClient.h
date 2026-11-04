#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QVariantMap>

// REST helper for the real app, promoted from
// ../../ui-interface-spike/rest_client.h (already proven against the real
// `violoop-edge` service - see that spike's docs/findings.md items B/C).
// Extended here with response headers and a raw byte count: the factory
// test audio_echo page's flow (POST /audio/factory-test/capture -> read
// the X-Saved/X-Capture-Ms response headers -> POST /audio/factory-test/
// play-file with that path) needs both, and `capture` returns WAV bytes,
// not JSON, so `body` (a best-effort UTF-8 decode, same as before) isn't a
// safe way to know how many bytes came back - rawSize is the actual byte
// count of the undecoded response.
class RestClient : public QObject
{
    Q_OBJECT
public:
    // baseUrlOverride, when non-empty, takes precedence over the
    // EDGE_API_BASE env var - used for the one endpoint that lives on a
    // different local service (violoop-edge-llm on :8000, see
    // TestLlmModelPage.qml), so that page gets its own RestClient instance
    // instead of this class growing a per-call base-URL parameter.
    explicit RestClient(QObject *parent = nullptr, const QString &baseUrlOverride = QString());

    Q_INVOKABLE void get(const QString &path);
    Q_INVOKABLE void postJson(const QString &path, const QString &jsonBody = QString());

signals:
    void requestSucceeded(const QString &path, int statusCode, const QString &body,
                           qint64 rawSize, const QVariantMap &headers);
    void requestFailed(const QString &path, const QString &errorString);

private:
    void send(const QString &path, const QByteArray &method, const QByteArray &body);

    QNetworkAccessManager m_nam;
    QUrl m_base;
};
