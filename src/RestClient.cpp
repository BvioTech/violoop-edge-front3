#include "RestClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>

RestClient::RestClient(QObject *parent, const QString &baseUrlOverride)
    : QObject(parent)
{
    if (!baseUrlOverride.isEmpty()) {
        m_base = QUrl(baseUrlOverride);
        return;
    }
    const QByteArray baseEnv = qgetenv("EDGE_API_BASE");
    m_base = QUrl(QString::fromLocal8Bit(baseEnv.isEmpty() ? "http://127.0.0.1:9090" : baseEnv));
}

void RestClient::get(const QString &path)
{
    send(path, "GET", QByteArray());
}

void RestClient::postJson(const QString &path, const QString &jsonBody)
{
    send(path, "POST", jsonBody.toUtf8());
}

void RestClient::send(const QString &path, const QByteArray &method, const QByteArray &body)
{
    // `path` may already carry a query string (e.g.
    // "/audio/factory-test/capture?ms=3000") - QUrl parses that out
    // correctly via setPath+setQuery below rather than folding it into the
    // path component.
    QUrl url = m_base;
    const int q = path.indexOf('?');
    if (q >= 0) {
        url.setPath(path.left(q));
        url.setQuery(path.mid(q + 1));
    } else {
        url.setPath(path);
    }

    QNetworkRequest req(url);
    // A handful of factory-test endpoints run long (wifi scan up to ~60s,
    // bluetooth scan up to 30s, the physical-button-press test up to 10s) -
    // generous enough to cover all of them rather than growing a
    // per-call timeout parameter for three call sites.
    req.setTransferTimeout(65000);
    if (!body.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = method == "POST"
        ? m_nam.post(req, body)
        : m_nam.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, path]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            emit requestFailed(path, reply->errorString());
            return;
        }

        const QByteArray raw = reply->readAll();
        QVariantMap headers;
        const auto pairs = reply->rawHeaderPairs();
        for (const auto &pair : pairs)
            headers.insert(QString::fromLatin1(pair.first), QString::fromUtf8(pair.second));

        emit requestSucceeded(path, status, QString::fromUtf8(raw), raw.size(), headers);
    });
}
