#ifndef AETHER_DBUS_SERVICE_H
#define AETHER_DBUS_SERVICE_H

#include "common.h"

void on_bus_acquired(GDBusConnection *c, const gchar *n, gpointer d);

#endif // AETHER_DBUS_SERVICE_H
