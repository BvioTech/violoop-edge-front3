#include "MascotInputs.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "MascotDirector.h"

namespace {
// Failed cloud probes in a row that make the device offline.
constexpr int kOfflineAfterFailedProbes = 2;
}

MascotInputs::MascotInputs(MascotDirector *director, int authorizationPollMs, QObject *parent)
    : QObject(parent)
    , m_director(director)
{
    m_authorizationPoll.setInterval(authorizationPollMs);
    connect(&m_authorizationPoll, &QTimer::timeout, this, &MascotInputs::authorizationStatusWanted);
}

void MascotInputs::connectivityProbed(bool ok)
{
    if (ok) {
        m_failedProbes = 0;
        m_director->clear(MascotDirector::Offline);
    } else if (++m_failedProbes >= kOfflineAfterFailedProbes) {
        m_failedProbes = kOfflineAfterFailedProbes; // no overflow over a long outage
        m_director->set(MascotDirector::Offline);
    }
}

void MascotInputs::eventsConnected()
{
    // Whatever happened while the stream was down was not replayed.
    emit authorizationStatusWanted();
}

void MascotInputs::eventReceived(const QString &type, const QString &data)
{
    if (type != QLatin1String("notification"))
        return;
    const QJsonObject event = QJsonDocument::fromJson(data.toUtf8()).object();
    if (event.value(QLatin1String("kind")).toString() != QLatin1String("authorization"))
        return;
    // A new request shows at once; anything else, and also whether the
    // new one is still there, is for the snapshot to say.
    if (event.value(QLatin1String("state")).toString() == QLatin1String("open"))
        setWaiting(true, event.value(QLatin1String("notifId")).toString());
    emit authorizationStatusWanted();
}

void MascotInputs::authorizationStatusReceived(int statusCode, const QString &body)
{
    // No answer, or 503 "auth broker not initialized": violoop-edge is
    // down or starting, and its requests did not survive that.
    QJsonObject active;
    if (statusCode == 200)
        active = QJsonDocument::fromJson(body.toUtf8()).object().value(QLatin1String("active")).toObject();
    setWaiting(!active.isEmpty(), active.value(QLatin1String("context")).toString());
}

void MascotInputs::setWaiting(bool waiting, const QString &request)
{
    if (waiting) {
        m_director->set(MascotDirector::WaitingUser, request);
        if (!m_authorizationPoll.isActive())
            m_authorizationPoll.start();
    } else {
        m_director->clear(MascotDirector::WaitingUser);
        m_authorizationPoll.stop();
    }
}
