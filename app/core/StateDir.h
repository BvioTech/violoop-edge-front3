#pragma once

#include <QString>

// Where the app keeps files that should outlive it (screenshots,
// performance recordings): the service's state directory - systemd's
// $STATE_DIRECTORY, i.e. /var/lib/violoop-edge-kiosk-qt, which survives
// reboots - or, when not run by that unit, the temp directory.
QString stateDirectory();
