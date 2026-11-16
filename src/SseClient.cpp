#include "SseClient.h"

#include <QDebug>
#include <QNetworkReply>
#include <QNetworkRequest>

SseClient::SseClient(QObject *parent)
    : QObject(parent)
{
    const QByteArray baseEnv = qgetenv("EDGE_API_BASE");
    QUrl base(QString::fromLocal8Bit(baseEnv.isEmpty() ? "http://127.0.0.1:9090" : baseEnv));
    m_url = base;
    m_url.setPath("/events");

    // Fixed short backoff, same as ../demos/ui-interface-spike/sse_client.cpp
    // (already verified against a real violoop-edge.service restart there -
    // see that spike's docs/findings.md 第四轮). Still not jittered/capped;
    // fine for this app's scale, revisit if reconnect storms become an
    // issue.
    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &SseClient::connectNow);
}

void SseClient::start()
{
    m_stopping = false;
    connectNow();
}

void SseClient::stop()
{
    m_stopping = true;
    m_reconnectTimer.stop();
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void SseClient::connectNow()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_buffer.clear();
    m_connectedFired = false;

    QNetworkRequest req(m_url);
    req.setRawHeader("Accept", "text/event-stream");
    // This is a long-lived streaming GET by design - a transfer timeout
    // would kill it the moment the server goes quiet between events/pings.
    req.setTransferTimeout(0);

    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, &SseClient::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &SseClient::onFinished);
}

void SseClient::scheduleReconnect(const QString &reason)
{
    emit disconnected(reason);
    if (m_stopping)
        return;
    m_reconnectTimer.start();
}

void SseClient::onReadyRead()
{
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();
    processBuffer();
}

void SseClient::onFinished()
{
    if (!m_reply)
        return;
    const QNetworkReply::NetworkError err = m_reply->error();
    const QString errorString = m_reply->errorString();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_stopping)
        return;
    scheduleReconnect(err == QNetworkReply::NoError
        ? QStringLiteral("server closed connection")
        : errorString);
}

void SseClient::processBuffer()
{
    // SSE frames are blank-line (\n\n) terminated; a frame may itself span
    // multiple \n-terminated lines (event:, data:, comments). Process every
    // complete frame currently in the buffer, leave a trailing partial
    // frame (if any) for the next readyRead.
    int sep;
    while ((sep = m_buffer.indexOf("\n\n")) != -1) {
        const QByteArray frame = m_buffer.left(sep);
        m_buffer.remove(0, sep + 2);
        dispatchFrame(frame);
    }
}

void SseClient::dispatchFrame(const QByteArray &frame)
{
    QString type;
    QStringList dataLines;

    const QList<QByteArray> lines = frame.split('\n');
    for (QByteArray line : lines) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.isEmpty())
            continue;
        if (line.startsWith(':')) {
            // Comment line - handshake (": connected") or heartbeat
            // (": ping"), per local-http-events.md. Neither carries an
            // event/data payload.
            if (line.contains("ping")) {
                emit heartbeat();
            } else if (line.contains("connected") && !m_connectedFired) {
                m_connectedFired = true;
                emit connected();
            }
            continue;
        }
        if (line.startsWith("event:")) {
            type = QString::fromUtf8(line.mid(6)).trimmed();
        } else if (line.startsWith("data:")) {
            dataLines << QString::fromUtf8(line.mid(5)).trimmed();
        }
        // Other field names (id:, retry:) are valid SSE but unused by this
        // protocol's documented event types - ignored rather than warned
        // on, since silently-ignoring unknown fields is correct SSE
        // client behavior per the spec, not a bug.
    }

    if (!m_connectedFired) {
        // First real event/data frame implies the stream is live even if
        // we somehow missed the ": connected" comment (e.g. it arrived in
        // the same readyRead chunk as this frame and got attributed to a
        // dead reply during a fast reconnect race) - don't under-report.
        m_connectedFired = true;
        emit connected();
    }

    if (!type.isEmpty() || !dataLines.isEmpty())
        emit eventReceived(type, dataLines.join('\n'));
}
