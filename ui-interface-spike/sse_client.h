#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

// Minimal hand-rolled text/event-stream client for validation item B in
// docs/findings.md: Qt has no built-in EventSource/SSE client, unlike the
// browser-ish guarantees edge-front2's SseClient (lib/services/sse_client.dart)
// gets from package:eventsource-equivalent handling. This implements just
// enough of the framing to talk to `violoop-edge`'s real `GET /events`
// (../../device/docs/protocols/local-http-events.md):
//   - lines starting with ":" are comments - the handshake sends
//     ": connected" once, then ": ping" every ~30s as a heartbeat. Neither
//     is an error and neither should trigger a reconnect.
//   - "event: <type>" / "data: <json>" lines, frames terminated by a blank
//     line.
// The protocol doc is explicit that the server does NOT replay missed
// events on reconnect and will drop frames for a slow consumer (backlog
// >=16) - so this class only proves the transport/parsing works; it is not
// a source of truth for application state (same rule edge-front2 follows:
// REST snapshots are authoritative, SSE is a best-effort hint).
class SseClient : public QObject
{
    Q_OBJECT
public:
    explicit SseClient(QObject *parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void connected();
    void disconnected(const QString &reason);
    void heartbeat();
    // data is the raw JSON text of the event's `data:` field(s), unparsed -
    // QML can JSON.parse() it. type is the `event:` field, or empty string
    // for comment-only frames (shouldn't normally reach here).
    void eventReceived(const QString &type, const QString &data);

private:
    void connectNow();
    void scheduleReconnect(const QString &reason);
    void onReadyRead();
    void onFinished();
    void processBuffer();
    void dispatchFrame(const QByteArray &frame);

    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_reply;
    QUrl m_url;
    QByteArray m_buffer;
    QTimer m_reconnectTimer;
    bool m_connectedFired = false;
    bool m_stopping = false;
};
