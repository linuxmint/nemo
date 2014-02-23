/*
 * nautilus-shell-search-provider.c - Implementation of a GNOME Shell
 *   search provider
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include <gio/gio.h>

#include "nautilus-shell-search-provider-generated.h"

#define SEARCH_PROVIDER_INACTIVITY_TIMEOUT 12000 /* milliseconds */

typedef struct {
  GApplication parent;

  guint name_owner_id;

  GDBusObjectManagerServer *object_manager;
  NautilusShellSearchProvider *skeleton;
} NautilusShellSearchProviderApp;

typedef GApplicationClass NautilusShellSearchProviderAppClass;

GType nautilus_shell_search_provider_app_get_type (void);

#define NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER_APP nautilus_shell_search_provider_app_get_type()
#define NAUTILUS_SHELL_SEARCH_PROVIDER_APP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER_APP, NautilusShellSearchProviderApp))

G_DEFINE_TYPE (NautilusShellSearchProviderApp, nautilus_shell_search_provider_app, G_TYPE_APPLICATION)

static void
handle_get_initial_result_set (GDBusMethodInvocation *invocation,
                               gchar                **terms)
{

}

static void
handle_get_subsearch_result_set (GDBusMethodInvocation *invocation,
                                 gchar                **previous_results,
                                 gchar                **terms)
{

}

static void
handle_get_result_metas (GDBusMethodInvocation *invocation,
                         gchar                **results)
{

}

static void
handle_activate_result (GDBusMethodInvocation *invocation,
                        gchar                 *result)
{

}

static void
search_provider_name_acquired_cb (GDBusConnection *connection,
                                  const gchar     *name,
                                  gpointer         user_data)
{
  g_debug ("Search provider name acquired: %s\n", name);
}

static void
search_provider_name_lost_cb (GDBusConnection *connection,
                              const gchar     *name,
                              gpointer         user_data)
{
  g_debug ("Search provider name lost: %s\n", name);
}

static void
search_provider_bus_acquired_cb (GDBusConnection *connection,
                                 const gchar *name,
                                 gpointer user_data)
{
  NautilusShellSearchProviderApp *self = user_data;

  self->object_manager = g_dbus_object_manager_server_new ("/org/gnome/Nautilus/SearchProvider");
  self->skeleton = nautilus_shell_search_provider_skeleton_new ();

  g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                    connection,
                                    "/org/gnome/Nautilus/SearchProvider", NULL);
  g_dbus_object_manager_server_set_connection (self->object_manager, connection);
}

static void
search_provider_app_dispose (GObject *obj)
{
  NautilusShellSearchProviderApp *self = NAUTILUS_SHELL_SEARCH_PROVIDER_APP (obj);

  if (self->name_owner_id != 0) {
    g_bus_unown_name (self->name_owner_id);
    self->name_owner_id = 0;
  }

  if (self->skeleton != NULL) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
    g_clear_object (&self->skeleton);
  }

  g_clear_object (&self->object_manager);

  G_OBJECT_CLASS (nautilus_shell_search_provider_app_parent_class)->dispose (obj);
}

static void
search_provider_app_startup (GApplication *app)
{
  NautilusShellSearchProviderApp *self = NAUTILUS_SHELL_SEARCH_PROVIDER_APP (app);

  G_APPLICATION_CLASS (nautilus_shell_search_provider_app_parent_class)->startup (app);

  /* hold indefinitely if we're asked to persist */
  if (g_getenv ("NAUTILUS_SEARCH_PROVIDER_PERSIST") != NULL)
    g_application_hold (app);

  self->name_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                        "org.gnome.Nautilus.SearchProvider",
                                        G_BUS_NAME_OWNER_FLAGS_NONE,
                                        search_provider_bus_acquired_cb,
                                        search_provider_name_acquired_cb,
                                        search_provider_name_lost_cb,
                                        app, NULL);
}

static void
nautilus_shell_search_provider_app_init (NautilusShellSearchProviderApp *self)
{
  GApplication *app = G_APPLICATION (self);

  g_application_set_inactivity_timeout (app, SEARCH_PROVIDER_INACTIVITY_TIMEOUT);
  g_application_set_application_id (app, "org.gnome.Nautilus.SearchProvider");
  g_application_set_flags (app, G_APPLICATION_IS_SERVICE);
}

static void
nautilus_shell_search_provider_app_class_init (NautilusShellSearchProviderAppClass *klass)
{
  GApplicationClass *aclass = G_APPLICATION_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  aclass->startup = search_provider_app_startup;
  oclass->dispose = search_provider_app_dispose;
}

static GApplication *
nautilus_shell_search_provider_app_new (void)
{
  g_type_init ();

  return g_object_new (nautilus_shell_search_provider_app_get_type (),
                       NULL);
}

int
main (int   argc,
      char *argv[])
{
  GApplication *app;
  gint res;

  app = nautilus_shell_search_provider_app_new ();
  res = g_application_run (app, argc, argv);
  g_object_unref (app);

  return res;
}

