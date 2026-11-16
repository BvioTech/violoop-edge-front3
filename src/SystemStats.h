#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// CPU load, GPU load and SoC temperature of the machine the app runs on,
// and this process's memory, read once a second straight from the kernel
// (/proc/stat, the GPU's devfreq node, the thermal zones,
// /proc/self/status) - for the home page's readout (qml/PerfOverlay.qml),
// next to the frame rate that counts itself, and for PerfRecorder.
//
// Only polls while `active` is set, so nothing is read when no page is
// showing the numbers. A value that can't be read on this machine (no
// such sysfs node - any dev PC) stays at -1.
class SystemStats : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    // Percent of all cores' time spent busy over the last second.
    Q_PROPERTY(qreal cpuLoad READ cpuLoad NOTIFY changed)
    // Percent, as the GPU's devfreq governor reports it.
    Q_PROPERTY(qreal gpuLoad READ gpuLoad NOTIFY changed)
    // Degrees Celsius, the "soc-thermal" zone.
    Q_PROPERTY(qreal socTemp READ socTemp NOTIFY changed)
    // Resident memory of this process, kB.
    Q_PROPERTY(qreal rssKb READ rssKb NOTIFY changed)

public:
    explicit SystemStats(QObject *parent = nullptr);

    bool active() const { return m_timer.isActive(); }
    void setActive(bool active);
    qreal cpuLoad() const { return m_cpuLoad; }
    qreal gpuLoad() const { return m_gpuLoad; }
    qreal socTemp() const { return m_socTemp; }
    qreal rssKb() const { return m_rssKb; }

signals:
    void activeChanged();
    void changed();

private:
    void poll();
    bool readCpuTimes(quint64 &busy, quint64 &total) const;

    QTimer m_timer;
    QString m_gpuLoadPath;
    QString m_socTempPath;
    quint64 m_lastBusy = 0;
    quint64 m_lastTotal = 0;
    bool m_haveLast = false;
    qreal m_cpuLoad = -1;
    qreal m_gpuLoad = -1;
    qreal m_socTemp = -1;
    qreal m_rssKb = -1;
};
