#include "JournalLog.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QtLogging>

#include <cstdio>
#include <unistd.h>

namespace {

bool g_prefixLines = false;

// syslog severities: 2 crit, 3 err, 4 warning, 6 info, 7 debug.
char severityFor(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return '7';
    case QtInfoMsg:
        return '6';
    case QtWarningMsg:
        return '4';
    case QtCriticalMsg:
        return '3';
    case QtFatalMsg:
        return '2';
    }
    return '6';
}

void journalMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QByteArray text = qFormatLogMessage(type, context, message).toLocal8Bit();
    if (!g_prefixLines) {
        fprintf(stderr, "%s\n", text.constData());
        return;
    }

    // journald reads the prefix per line, so a multi-line message needs it
    // on every line or the later lines fall back to info.
    const char prefix[] = { '<', severityFor(type), '>', '\0' };
    QByteArray out;
    for (const QByteArray &line : text.split('\n')) {
        out += prefix;
        out += line;
        out += '\n';
    }
    // One write per message keeps lines from different threads apart.
    fwrite(out.constData(), 1, out.size(), stderr);
}

}

void installJournalLogHandler()
{
    g_prefixLines = !isatty(STDERR_FILENO);
    qInstallMessageHandler(journalMessageHandler);
}
