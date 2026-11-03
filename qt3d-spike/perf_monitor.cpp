#include "perf_monitor.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QThread>

#include <unistd.h>

// Env var overrides, in case this ever runs on hardware other than dev1
// (rk3576/Mali-G52). Defaults match what's actually present on dev1 - see
// the "GPU/thermal sysfs" probe in the chat history that found these paths.
static QByteArray envOr(const char *name, const char *fallback)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? QByteArray(fallback) : v;
}

PerfMonitor::PerfMonitor(QObject *parent)
    : QObject(parent)
    , m_gpuUtilPath(envOr("QT3D_SPIKE_GPU_UTIL_PATH", "/sys/devices/platform/27800000.gpu/utilisation"))
    , m_gpuTempPath(envOr("QT3D_SPIKE_GPU_TEMP_PATH", "/sys/class/thermal/thermal_zone5/temp"))
    , m_socTempPath(envOr("QT3D_SPIKE_SOC_TEMP_PATH", "/sys/class/thermal/thermal_zone0/temp"))
{
    m_numCpus = qMax(1, int(QThread::idealThreadCount()));
    const long ticks = sysconf(_SC_CLK_TCK);
    if (ticks > 0)
        m_clockTicksPerSec = ticks;

    const QByteArray logPathEnv = qgetenv("QT3D_SPIKE_PERF_LOG");
    const QString logPath = logPathEnv.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("qt3d_spike_perf.csv")
        : QString::fromLocal8Bit(logPathEnv);

    m_logFile.setFileName(logPath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_logFile.write("timestamp,elapsed_s,fps,sync_ms,render_ms,cpu_pct,rss_kb,gpu_pct,soc_temp_c,gpu_temp_c\n");
        m_logFile.flush();
        qInfo() << "PerfMonitor: logging to" << logPath;
    } else {
        qWarning() << "PerfMonitor: could not open log file" << logPath << m_logFile.errorString();
    }

    m_wallTimer.start();
    qint64 u = 0, s = 0;
    if (readProcSelfCpuTicks(&u, &s))
        m_lastCpuTicks = u + s;
    m_lastWallMs = m_wallTimer.elapsed();
}

bool PerfMonitor::readProcSelfCpuTicks(qint64 *utime, qint64 *stime) const
{
    QFile f(QStringLiteral("/proc/self/stat"));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray line = f.readAll();
    // Fields after the last ')' (comm can contain spaces/parens), 0-indexed
    // from "state": state(0) ppid(1) pgrp(2) session(3) tty_nr(4) tpgid(5)
    // flags(6) minflt(7) cminflt(8) majflt(9) cmajflt(10) utime(11) stime(12)
    const int close = line.lastIndexOf(')');
    if (close < 0)
        return false;
    const QByteArrayList fields = line.mid(close + 1).simplified().split(' ');
    if (fields.size() <= 12)
        return false;
    if (utime)
        *utime = fields.at(11).toLongLong();
    if (stime)
        *stime = fields.at(12).toLongLong();
    return true;
}

double PerfMonitor::sampleCpuPercent()
{
    qint64 u = 0, s = 0;
    if (!readProcSelfCpuTicks(&u, &s))
        return -1.0;
    const qint64 nowTicks = u + s;
    const qint64 nowMs = m_wallTimer.elapsed();

    const qint64 deltaTicks = nowTicks - m_lastCpuTicks;
    const qint64 deltaMs = nowMs - m_lastWallMs;
    m_lastCpuTicks = nowTicks;
    m_lastWallMs = nowMs;

    if (deltaMs <= 0)
        return -1.0;
    const double deltaCpuMs = (double(deltaTicks) / double(m_clockTicksPerSec)) * 1000.0;
    // Percent of a single core (matches `top`/`ps` convention - can exceed
    // 100% for a multi-threaded process; divide by m_numCpus for percent of
    // total machine capacity instead).
    return (deltaCpuMs / double(deltaMs)) * 100.0;
}

double PerfMonitor::sampleRssKb()
{
    // /proc files report size 0 and QIODevice::atEnd()/readLine() are
    // unreliable on them (classic Qt-on-procfs gotcha) - read it all in one
    // go instead, same as readProcSelfCpuTicks() does for /proc/self/stat.
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly))
        return -1.0;
    const QByteArrayList lines = f.readAll().split('\n');
    for (const QByteArray &line : lines) {
        if (line.startsWith("VmRSS:")) {
            const QByteArrayList parts = line.simplified().split(' ');
            if (parts.size() >= 2)
                return parts.at(1).toDouble();
        }
    }
    return -1.0;
}

double PerfMonitor::readUtilisationPercent(const QByteArray &path)
{
    QFile f(QString::fromLocal8Bit(path));
    if (!f.open(QIODevice::ReadOnly))
        return -1.0;
    bool ok = false;
    const double v = f.readAll().trimmed().toDouble(&ok);
    return ok ? v : -1.0;
}

double PerfMonitor::readTempCelsius(const QByteArray &path)
{
    QFile f(QString::fromLocal8Bit(path));
    if (!f.open(QIODevice::ReadOnly))
        return -1.0;
    bool ok = false;
    // Linux thermal_zone*/temp is millidegrees C.
    const double milli = f.readAll().trimmed().toDouble(&ok);
    return ok ? milli / 1000.0 : -1.0;
}

void PerfMonitor::logSample(double fps, double syncTimeMs, double renderTimeMs)
{
    const double cpuPct = sampleCpuPercent();
    const double rssKb = sampleRssKb();
    const double gpuPct = readUtilisationPercent(m_gpuUtilPath);
    const double socTemp = readTempCelsius(m_socTempPath);
    const double gpuTemp = readTempCelsius(m_gpuTempPath);
    const double elapsedS = m_wallTimer.elapsed() / 1000.0;

    const QString line = QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs))
        .arg(elapsedS, 0, 'f', 1)
        .arg(fps, 0, 'f', 1)
        .arg(syncTimeMs, 0, 'f', 2)
        .arg(renderTimeMs, 0, 'f', 2)
        .arg(cpuPct, 0, 'f', 1)
        .arg(rssKb, 0, 'f', 0)
        .arg(gpuPct, 0, 'f', 1)
        .arg(socTemp, 0, 'f', 1)
        .arg(gpuTemp, 0, 'f', 1);

    if (m_logFile.isOpen()) {
        m_logFile.write(line.toUtf8());
        m_logFile.flush();
    }

    qInfo().noquote() << "perf:" << line.trimmed();
}
