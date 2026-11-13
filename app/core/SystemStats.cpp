#include "SystemStats.h"

#include <QDir>
#include <QFile>

namespace {

QByteArray readFirstLine(const QString &path)
{
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readLine().trimmed();
}

// The GPU's devfreq node, e.g. /sys/class/devfreq/27800000.gpu/load on
// rk3576 (Mali). Its content looks like "53@900000000Hz".
QString findGpuLoadPath()
{
    const QDir dir(QStringLiteral("/sys/class/devfreq"));
    const auto entries = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        const QString path = dir.filePath(entry) + QStringLiteral("/load");
        if (entry.contains(QLatin1String("gpu")) && QFile::exists(path))
            return path;
    }
    return QString();
}

QString findSocTempPath()
{
    const QDir dir(QStringLiteral("/sys/class/thermal"));
    const auto zones = dir.entryList({ QStringLiteral("thermal_zone*") }, QDir::Dirs);
    for (const QString &zone : zones) {
        if (readFirstLine(dir.filePath(zone) + QStringLiteral("/type")) == "soc-thermal")
            return dir.filePath(zone) + QStringLiteral("/temp");
    }
    return QString();
}

} // namespace

SystemStats::SystemStats(QObject *parent)
    : QObject(parent)
    , m_gpuLoadPath(findGpuLoadPath())
    , m_socTempPath(findSocTempPath())
{
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &SystemStats::poll);
}

void SystemStats::setActive(bool active)
{
    if (active == m_timer.isActive())
        return;
    if (active) {
        // CPU load is a difference between two readings: take the first
        // one now, the first value follows a second later.
        m_haveLast = readCpuTimes(m_lastBusy, m_lastTotal);
        m_timer.start();
        poll();
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

bool SystemStats::readCpuTimes(quint64 &busy, quint64 &total) const
{
    // "cpu  user nice system idle iowait irq softirq steal ..."
    const auto fields = readFirstLine(QStringLiteral("/proc/stat")).simplified().split(' ');
    if (fields.size() < 6 || fields.at(0) != "cpu")
        return false;
    total = 0;
    for (int i = 1; i < fields.size() && i <= 8; ++i)
        total += fields.at(i).toULongLong();
    busy = total - fields.at(4).toULongLong() - fields.at(5).toULongLong();
    return true;
}

void SystemStats::poll()
{
    quint64 busy = 0;
    quint64 total = 0;
    if (readCpuTimes(busy, total)) {
        if (m_haveLast && total > m_lastTotal)
            m_cpuLoad = 100.0 * qreal(busy - m_lastBusy) / qreal(total - m_lastTotal);
        m_lastBusy = busy;
        m_lastTotal = total;
        m_haveLast = true;
    }

    bool ok = false;
    const QByteArray load = readFirstLine(m_gpuLoadPath);
    const qreal gpu = load.left(load.indexOf('@') < 0 ? load.size() : load.indexOf('@')).toDouble(&ok);
    m_gpuLoad = ok ? gpu : -1;

    const qreal milli = readFirstLine(m_socTempPath).toDouble(&ok);
    m_socTemp = ok ? milli / 1000.0 : -1;

    emit changed();
}
