#include "PerfRecorder.h"

#include "SystemStats.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace {

// Empty for "could not be read" (SystemStats reports those as -1).
QByteArray field(qreal value, int decimals)
{
    return value < 0 ? QByteArray() : QByteArray::number(value, 'f', decimals);
}

} // namespace

PerfRecorder::PerfRecorder(SystemStats *stats, QObject *parent)
    : QObject(parent)
    , m_stats(stats)
{
}

bool PerfRecorder::start()
{
    stop();
    // systemd may list several directories, colon-separated; ours has one.
    QString dir = QString::fromLocal8Bit(qgetenv("STATE_DIRECTORY")).section(QLatin1Char(':'), 0, 0);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString name = QStringLiteral("away-perf-%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    m_file.setFileName(QDir(dir).filePath(name));
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning() << "PerfRecorder: could not open" << m_file.fileName() << "-" << m_file.errorString();
        return false;
    }
    m_file.write("time,elapsed_s,clip,paused,fps,cpu_pct,gpu_pct,soc_temp_c,rss_kb\n");
    m_file.flush();
    m_elapsed.start();
    qInfo() << "PerfRecorder: recording to" << m_file.fileName();
    emit fileChanged();
    return true;
}

void PerfRecorder::stop()
{
    if (!m_file.isOpen())
        return;
    m_file.close();
    emit fileChanged();
}

void PerfRecorder::record(qreal fps, const QString &clip, bool paused)
{
    if (!m_file.isOpen())
        return;
    const QList<QByteArray> fields = {
        QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1(),
        QByteArray::number(m_elapsed.elapsed() / 1000),
        // Clip names come from the model file: keep them from breaking
        // the line.
        QString(clip).replace(QLatin1Char(','), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' ')).toUtf8(),
        paused ? "1" : "0",
        field(fps, 1),
        field(m_stats->cpuLoad(), 1),
        field(m_stats->gpuLoad(), 0),
        field(m_stats->socTemp(), 1),
        field(m_stats->rssKb(), 0),
    };
    m_file.write(fields.join(',') + '\n');
    m_file.flush();
}
