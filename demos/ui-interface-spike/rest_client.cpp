#include "rest_client.h"

#include <QNetworkReply>
#include <QNetworkRequest>

RestClient::RestClient(QObject *parent)
    : QObject(parent)
{
    const QByteArray baseEnv = qgetenv("UI_SPIKE_EDGE_BASE");
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
    QUrl url = m_base;
    url.setPath(path);

    QNetworkRequest req(url);
    req.setTransferTimeout(5000);
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
        emit requestSucceeded(path, status, QString::fromUtf8(reply->readAll()));
    });
}
