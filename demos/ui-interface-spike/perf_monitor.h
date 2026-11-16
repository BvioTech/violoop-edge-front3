#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QFile>

// Periodic performance sampler for the ui-interface-spike real-hardware
// tests (see docs/findings.md, validation item F). Copied near-verbatim
// from ../qt3d-spike/perf_monitor.h, with env var names changed to avoid
// colliding with that spike's if both are ever run on the same box - the
// sampling approach (procfs/sysfs, Mali/rk3576-specific GPU paths) is
// already validated there and reused as-is rather than re-derived.
//
// Unlike qt3d-spike, this scene has no Quick3D View3D to source fps/sync/
// render timings from, so logSample()'s first argument is a plain QML-side
// frame counter (see combined.qml/VoiceStateView.qml) rather than
// Quick3D's renderStats; syncTimeMs/renderTimeMs are unused here (always
// logged as 0) and kept only so the CSV column layout matches qt3d-spike's
// for easy comparison.
class PerfMonitor : public QObject
{
    Q_OBJECT
public:
    explicit PerfMonitor(QObject *parent = nullptr);

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
