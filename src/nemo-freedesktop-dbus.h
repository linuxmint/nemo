/*
 * nemo-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */


#ifndef __NEMO_FREEDESKTOP_DBUS_H__
#define __NEMO_FREEDESKTOP_DBUS_H__

#include <glib-object.h>

#define NEMO_FDO_DBUS_IFACE "org.freedesktop.FileManager1"
#define NEMO_FDO_DBUS_NAME  "org.freedesktop.FileManager1"
#define NEMO_FDO_DBUS_PATH  "/org/freedesktop/FileManager1"

#define NEMO_TYPE_FREEDESKTOP_DBUS nemo_freedesktop_dbus_get_type()
#define NEMO_FREEDESKTOP_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_FREEDESKTOP_DBUS, NautilusFreedesktopDBus))
#define NEMO_FREEDESKTOP_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_FREEDESKTOP_DBUS, NautilusFreedesktopDBusClass))
#define NEMO_IS_FREEDESKTOP_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_FREEDESKTOP_DBUS))
#define NEMO_IS_FREEDESKTOP_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_FREEDESKTOP_DBUS))
#define NEMO_FREEDESKTOP_DBUS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_FREEDESKTOP_DBUS, NautilusFreedesktopDBusClass))

typedef struct _NemoFreedesktopDBus NemoFreedesktopDBus;
typedef struct _NemoFreedesktopDBusClass NemoFreedesktopDBusClass;

GType nemo_freedesktop_dbus_get_type (void);
NemoFreedesktopDBus * nemo_freedesktop_dbus_new (void);

void nemo_freedesktop_dbus_set_open_locations (NemoFreedesktopDBus *fdb, const gchar **locations);

#endif /* __NEMO_FREEDESKTOP_DBUS_H__ */
