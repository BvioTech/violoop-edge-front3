#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QFile>

// Periodic performance sampler for the qt3d-spike real-hardware tests
// (see docs/findings.md). Exposed to QML as a context property; QML drives
// the sampling interval and supplies the Quick3D-side numbers (fps/sync/
// render time) that aren't available from outside the render thread.
//
// CPU%, RSS, GPU utilization and temperatures are read directly from
// procfs/sysfs here in C++ since QML has no access to those. GPU paths are
// Mali/rk3576-specific (this device); overridable via env vars so the same
// binary doesn't silently misreport on different hardware.
class PerfMonitor : public QObject
{
    Q_OBJECT
public:
    explicit PerfMonitor(QObject *parent = nullptr);

    // Called from QML on a Timer tick with the current Quick3D render stats.
    Q_INVOKABLE void logSample(double fps, double syncTimeMs, double renderTimeMs);

private:
    double sampleCpuPercent();
    double sampleRssKb();
    double readUtilisationPercent(const QByteArray &path);
    double readTempCelsius(const QByteArray &path);
    bool readProcSelfCpuTicks(qint64 *utime, qint64 *stime) const;

    QFile m_logFile;
    QElapsedTimer m_wallTimer;
    qint64 m_lastWallMs = 0;
    qint64 m_lastCpuTicks = 0;
    long m_clockTicksPerSec = 100;
    int m_numCpus = 1;

    QByteArray m_gpuUtilPath;
    QByteArray m_gpuTempPath;
    QByteArray m_socTempPath;
};
