#pragma once

// journald records everything a service writes to stderr as "info" unless
// each line starts with a "<N>" syslog severity prefix. The log shipper
// filters on that priority (this unit only ships warning and above), so
// without the prefix a qWarning never reaches the cloud. See
// os-next/docs/app-logging-self-audit.md, section 5.
//
// Installs a Qt message handler that adds the prefix to every line. It
// covers qDebug/qInfo/qWarning/qCritical, QML console.*, and Qt's own
// warnings. When stderr is a terminal the prefix is left out, so a
// hand-run build still prints plain lines.
void installJournalLogHandler();
