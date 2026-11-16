#include "StateDir.h"

#include <QStandardPaths>

QString stateDirectory()
{
    // systemd may list several directories, colon-separated; ours has one.
    const QString dir = QString::fromLocal8Bit(qgetenv("STATE_DIRECTORY")).section(QLatin1Char(':'), 0, 0);
    return dir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::TempLocation) : dir;
}
