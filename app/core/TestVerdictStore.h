#pragma once

#include <QObject>
#include <QVariantMap>
#include <QVector>

class RestClient;
class StatusStore;

// Equivalent of edge-front2's lib/stores/test_store.dart verdict/sync half
// (not the live-reading half - each page fetches its own live data, same
// as Flutter's TestPage sub-pages do). Holds the pass/fail/pending state
// for all factory-test sections in the SAME canonical order and id
// strings as test_store.dart's `sections` getter, and pushes the same
// wire shape to `POST /factory-test/data` that Flutter's `_syncToBackend()`/
// `_snapshot()` do (confirmed against ../../device/server/transports/
// factory_data_handler.js - the backend itself treats `data` as opaque,
// but an external QA tool reads this same slot expecting Flutter's shape,
// so matching it exactly is the actual "business alignment" requirement,
// not just satisfying the backend's own validation).
//
// `usbInput` intentionally has its own test page but is NOT in the id
// list below - matches test_store.dart's comment "usbInputTest 已停用：
// 不进入汇总，也不计入 summary 的 total/pending".
class TestVerdictStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap verdicts READ verdicts NOTIFY verdictsChanged)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY verdictsChanged)

public:
    explicit TestVerdictStore(RestClient *rest, QObject *parent = nullptr);

    // Kept decoupled from StatusStore (no C++ dependency) - AppShell.qml
    // wires `statusStore.deviceSnChanged -> testVerdictStore.setDeviceSn`
    // once at startup instead. Falls back to "unknown" (matching
    // test_store.dart) until the first /status/system read lands.
    Q_INVOKABLE void setDeviceSn(const QString &sn) { m_deviceSn = sn.isEmpty() ? QStringLiteral("unknown") : sn; }

    // status is "passed" | "failed" | "pending" (same strings Flutter's
    // TestStatus.name produces).
    Q_INVOKABLE void setVerdict(const QString &id, const QString &status);
    // Stashes the captured reading for a data-bearing section (device,
    // memory, computeCard, llmModel, wifi, bluetooth, power, hdmi) - sent
    // verbatim in the next sync, same as Flutter's `_dataFor()`.
    Q_INVOKABLE void setData(const QString &id, const QVariant &data);
    Q_INVOKABLE QString verdict(const QString &id) const;
    Q_INVOKABLE void reset();

    // VLA11-only gate for the LLM model section - call once boardProfile
    // is known (see StatusStore::systemChanged). Safe to call repeatedly;
    // only changes the snapshot/summary, not already-recorded verdicts.
    Q_INVOKABLE void setLlmModelVisible(bool visible);

    QVariantMap verdicts() const;
    QVariantMap summary() const;

signals:
    void verdictsChanged();

private:
    struct Item
    {
        QString id;
        QString status = QStringLiteral("pending");
        QVariant data;
    };

    void sync();
    int indexOf(const QString &id) const;

    RestClient *m_rest;
    QString m_deviceSn = QStringLiteral("unknown");
    QVector<Item> m_items;
    bool m_llmModelVisible = false;
};
