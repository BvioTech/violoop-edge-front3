#pragma once

#include <QElapsedTimer>
#include <QFile>
#include <QObject>
#include <QString>

class SystemStats;

// Writes the away page's performance readout to a CSV file, one line a
// second, for long unattended runs (burn-in): is the frame rate holding,
// is memory creeping up, where does the temperature settle.
//
// Recording is off unless asked for, so that a device in normal use does
// not fill its state directory:
//
//   sudo kill -USR2 "$(systemctl show -p MainPID --value violoop-edge-kiosk-qt)"
//
// starts it, and the same command again stops it (main.cpp, through
// UnixSignalWatcher). EDGE_KIOSK_RECORD_PERF=1 in the service's
// environment starts it with the app, for runs nobody is watching.
//
// qml/AwayPage.qml feeds it: record() on the same one-second tick that
// updates the on-screen numbers (the page supplies what only it knows -
// frame rate and the clip playing - the rest comes from SystemStats).
// Nothing is written while that page is not open.
//
// Every start() begins a new file, away-perf-<date>-<time>.csv, in the
// service's state directory (systemd's $STATE_DIRECTORY, i.e.
// /var/lib/violoop-edge-kiosk-qt - it survives reboots) or, when not run
// by that unit, the temp directory. Each line is flushed as it is
// written, so a run cut short by a crash or power loss keeps everything
// up to its last second. Columns:
//
//   time        local time, ISO 8601
//   elapsed_s   seconds since start()
//   clip        the animation clip playing
//   paused      1 while the animation is paused
//   fps         frames put on screen over the last second
//   cpu_pct     all cores, see SystemStats
//   gpu_pct
//   soc_temp_c
//   rss_kb      this process's resident memory
//
// A value that could not be read is written as an empty field.
class PerfRecorder : public QObject
{
    Q_OBJECT
    // Path of the file being written, empty when not recording.
    Q_PROPERTY(QString file READ file NOTIFY fileChanged)

public:
    explicit PerfRecorder(SystemStats *stats, QObject *parent = nullptr);

    QString file() const { return m_file.isOpen() ? m_file.fileName() : QString(); }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    // start() when not recording, stop() when recording.
    void toggle();
    Q_INVOKABLE void record(qreal fps, const QString &clip, bool paused);

signals:
    void fileChanged();

private:
    SystemStats *m_stats;
    QFile m_file;
    QElapsedTimer m_elapsed;
};
