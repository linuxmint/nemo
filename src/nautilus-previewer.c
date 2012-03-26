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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "nautilus-previewer.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_PREVIEWER
#include <libnautilus-private/nautilus-debug.h>

#include <gio/gio.h>

G_DEFINE_TYPE (NautilusPreviewer, nautilus_previewer, G_TYPE_OBJECT);

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static NautilusPreviewer *singleton = NULL;

struct _NautilusPreviewerPriv {
  GDBusConnection *connection;
};

static void
nautilus_previewer_dispose (GObject *object)
{
  NautilusPreviewer *self = NAUTILUS_PREVIEWER (object);

  DEBUG ("%p", self);

  g_clear_object (&self->priv->connection);

  G_OBJECT_CLASS (nautilus_previewer_parent_class)->dispose (object);
}

static GObject *
nautilus_previewer_constructor (GType type,
                                guint n_construct_params,
                                GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (singleton != NULL)
    return G_OBJECT (singleton);

  retval = G_OBJECT_CLASS (nautilus_previewer_parent_class)->constructor
    (type, n_construct_params, construct_params);

  singleton = NAUTILUS_PREVIEWER (retval);
  g_object_add_weak_pointer (retval, (gpointer) &singleton);

  return retval;
}

static void
nautilus_previewer_init (NautilusPreviewer *self)
{
  GError *error = NULL;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_PREVIEWER,
                                            NautilusPreviewerPriv);

  self->priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                                           NULL, &error);

  if (error != NULL) {
    g_printerr ("Unable to initialize DBus connection: %s", error->message);
    g_error_free (error);
    return;
  }
}

static void
nautilus_previewer_class_init (NautilusPreviewerClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->constructor = nautilus_previewer_constructor;
  oclass->dispose = nautilus_previewer_dispose;

  g_type_class_add_private (klass, sizeof (NautilusPreviewerPriv));
}

static void
previewer_show_file_ready_cb (GObject *source,
                              GAsyncResult *res,
                              gpointer user_data)
{
  NautilusPreviewer *self = user_data;
  GError *error = NULL;

  g_dbus_connection_call_finish (self->priv->connection,
                                 res, &error);

  if (error != NULL) {
    DEBUG ("Unable to call ShowFile on NautilusPreviewer: %s",
           error->message);
    g_error_free (error);
  }

  g_object_unref (self);
}

static void
previewer_close_ready_cb (GObject *source,
                          GAsyncResult *res,
                          gpointer user_data)
{
  NautilusPreviewer *self = user_data;
  GError *error = NULL;

  g_dbus_connection_call_finish (self->priv->connection,
                                 res, &error);

  if (error != NULL) {
    DEBUG ("Unable to call Close on NautilusPreviewer: %s",
           error->message);
    g_error_free (error);
  }

  g_object_unref (self);
}

NautilusPreviewer *
nautilus_previewer_get_singleton (void)
{
  return g_object_new (NAUTILUS_TYPE_PREVIEWER, NULL);
}

void
nautilus_previewer_call_show_file (NautilusPreviewer *self,
                                   const gchar *uri,
                                   guint xid,
				   gboolean close_if_already_visible)
{
  GVariant *variant;

  variant = g_variant_new ("(sib)",
                           uri, xid, close_if_already_visible);

  if (self->priv->connection == NULL) {
    g_printerr ("No DBus connection available");
    return;
  }

  g_dbus_connection_call (self->priv->connection,
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
                          g_object_ref (self));
}

void
nautilus_previewer_call_close (NautilusPreviewer *self)
{
  if (self->priv->connection == NULL) {
    g_printerr ("No DBus connection available");
    return;
  }

  /* don't autostart the previewer if it's not running */
  g_dbus_connection_call (self->priv->connection,
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
                          g_object_ref (self));
}
