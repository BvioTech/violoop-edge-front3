#include "TestVerdictStore.h"
#include "RestClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

TestVerdictStore::TestVerdictStore(RestClient *rest, QObject *parent)
    : QObject(parent)
    , m_rest(rest)
{
    // Canonical order from edge-front2's lib/stores/test_store.dart
    // `sections` getter (llmModel appended separately, gated by
    // setLlmModelVisible()).
    static const char *kIds[] = {
        "device", "memory", "sn", "display", "touch", "backlight", "fan",
        "light", "audio", "audioEcho", "mcuPing", "button", "wifi",
        "bluetooth", "power", "usb", "hdmi", "computeCard",
    };
    for (const char *id : kIds)
        m_items.append(Item{QString::fromLatin1(id), QStringLiteral("pending"), QVariant()});
}

int TestVerdictStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].id == id)
            return i;
    return -1;
}

void TestVerdictStore::setVerdict(const QString &id, const QString &status)
{
    const int i = indexOf(id);
    if (i < 0) {
        qWarning("TestVerdictStore: unknown test id %s", qPrintable(id));
        return;
    }
    if (m_items[i].status == status)
        return;
    m_items[i].status = status;
    emit verdictsChanged();
    sync();
}

void TestVerdictStore::setData(const QString &id, const QVariant &data)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    m_items[i].data = data;
    // Data alone doesn't need to repaint verdict dots, but it does need to
    // reach the backend snapshot.
    sync();
}

QString TestVerdictStore::verdict(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? QStringLiteral("pending") : m_items[i].status;
}

void TestVerdictStore::reset()
{
    for (Item &item : m_items) {
        item.status = QStringLiteral("pending");
        item.data = QVariant();
    }
    emit verdictsChanged();
    sync();
}

void TestVerdictStore::setLlmModelVisible(bool visible)
{
    if (m_llmModelVisible == visible)
        return;
    m_llmModelVisible = visible;
    if (visible && indexOf(QStringLiteral("llmModel")) < 0)
        m_items.append(Item{QStringLiteral("llmModel"), QStringLiteral("pending"), QVariant()});
    // Not removed if it flips back false after being shown once - matches
    // the spirit of Flutter's "never seen until a VLA11 unit confirms it"
    // (board profile doesn't change at runtime in practice).
    emit verdictsChanged();
}

QVariantMap TestVerdictStore::verdicts() const
{
    QVariantMap m;
    for (const Item &item : m_items)
        m.insert(item.id, item.status);
    return m;
}

QVariantMap TestVerdictStore::summary() const
{
    int passed = 0, failed = 0, pending = 0;
    for (const Item &item : m_items) {
        if (item.status == "passed") ++passed;
        else if (item.status == "failed") ++failed;
        else ++pending;
    }
    QVariantMap m;
    m["total"] = m_items.size();
    m["passed"] = passed;
    m["failed"] = failed;
    m["pending"] = pending;
    return m;
}

void TestVerdictStore::sync()
{
    QJsonArray results;
    for (const Item &item : m_items) {
        QJsonObject r;
        r["id"] = item.id;
        r["status"] = item.status;
        if (item.data.isValid())
            r["data"] = QJsonValue::fromVariant(item.data);
        results.append(r);
    }

    const QVariantMap s = summary();
    QJsonObject summaryObj;
    summaryObj["total"] = s["total"].toInt();
    summaryObj["passed"] = s["passed"].toInt();
    summaryObj["failed"] = s["failed"].toInt();
    summaryObj["pending"] = s["pending"].toInt();

    QJsonObject data;
    data["results"] = results;
    data["summary"] = summaryObj;

    QJsonObject body;
    body["sn"] = m_deviceSn;
    body["data"] = data;

    // Best-effort, same as Flutter's _syncToBackend(): errors are
    // swallowed by RestClient's normal requestFailed signal going
    // unhandled here - the next verdict change re-pushes full state, so a
    // dropped push self-heals.
    m_rest->postJson(QStringLiteral("/factory-test/data"), QString::fromUtf8(QJsonDocument(body).toJson(QJsonDocument::Compact)));
}
