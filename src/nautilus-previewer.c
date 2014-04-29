/*
 * nautilus-previewer: nautilus previewer DBus wrapper
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "config.h"

#include "nautilus-previewer.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_PREVIEWER
#include <libnautilus-private/nautilus-debug.h>

#include <gio/gio.h>

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static void
previewer_show_file_ready_cb (GObject *source,
                              GAsyncResult *res,
                              gpointer user_data)
{
  GError *error = NULL;

  g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                 res, &error);

  if (error != NULL) {
    DEBUG ("Unable to call ShowFile on NautilusPreviewer: %s",
           error->message);
    g_error_free (error);
  }
}

static void
previewer_close_ready_cb (GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
  GError *error = NULL;

  g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                 res, &error);

  if (error != NULL) {
    DEBUG ("Unable to call Close on NautilusPreviewer: %s",
           error->message);
    g_error_free (error);
  }
}

void
nautilus_previewer_call_show_file (const gchar *uri,
                                   guint xid,
				   gboolean close_if_already_visible)
{
  GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());
  GVariant *variant;

  variant = g_variant_new ("(sib)",
                           uri, xid, close_if_already_visible);

  g_dbus_connection_call (connection,
                          PREVIEWER_DBUS_NAME,
                          PREVIEWER_DBUS_PATH,
                          PREVIEWER_DBUS_IFACE,
                          "ShowFile",
                          variant,
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          previewer_show_file_ready_cb,
                          NULL);
}

void
nautilus_previewer_call_close (void)
{
  GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());

  /* don't autostart the previewer if it's not running */
  g_dbus_connection_call (connection,
                          PREVIEWER_DBUS_NAME,
                          PREVIEWER_DBUS_PATH,
                          PREVIEWER_DBUS_IFACE,
                          "Close",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          NULL,
                          previewer_close_ready_cb,
                          NULL);
}
