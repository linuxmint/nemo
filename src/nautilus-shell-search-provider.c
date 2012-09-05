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

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-search-provider.h>

#include "nautilus-shell-search-provider-generated.h"

#define SEARCH_PROVIDER_INACTIVITY_TIMEOUT 12000 /* milliseconds */

typedef struct {
  NautilusSearchEngine *engine;
  NautilusQuery *query;

  GHashTable *hits;
  GDBusMethodInvocation *invocation;

  gint64 start_time;
} PendingSearch;

typedef struct {
  GApplication parent;

  guint name_owner_id;
  GDBusObjectManagerServer *object_manager;
  NautilusShellSearchProvider *skeleton;

  PendingSearch *current_search;
} NautilusShellSearchProviderApp;

typedef GApplicationClass NautilusShellSearchProviderAppClass;

GType nautilus_shell_search_provider_app_get_type (void);

#define NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER_APP nautilus_shell_search_provider_app_get_type()
#define NAUTILUS_SHELL_SEARCH_PROVIDER_APP(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SHELL_SEARCH_PROVIDER_APP, NautilusShellSearchProviderApp))

G_DEFINE_TYPE (NautilusShellSearchProviderApp, nautilus_shell_search_provider_app, G_TYPE_APPLICATION)

static GVariant *
variant_from_pixbuf (GdkPixbuf *pixbuf)
{
  GVariant *variant;
  guchar *data;
  guint   length;

  data = gdk_pixbuf_get_pixels_with_length (pixbuf, &length);
  variant = g_variant_new ("(iiibii@ay)",
                           gdk_pixbuf_get_width (pixbuf),
                           gdk_pixbuf_get_height (pixbuf),
                           gdk_pixbuf_get_rowstride (pixbuf),
                           gdk_pixbuf_get_has_alpha (pixbuf),
                           gdk_pixbuf_get_bits_per_sample (pixbuf),
                           gdk_pixbuf_get_n_channels (pixbuf),
                           g_variant_new_from_data (G_VARIANT_TYPE_BYTESTRING,
                                                    data, length, TRUE,
                                                    (GDestroyNotify)g_object_unref,
                                                    g_object_ref (pixbuf)));
  return variant;
}

static void
pending_search_free (PendingSearch *search)
{
  g_hash_table_destroy (search->hits);
  g_clear_object (&search->query);
  g_clear_object (&search->engine);

  g_slice_free (PendingSearch, search);
}

static void
finish_current_search (NautilusShellSearchProviderApp *self,
                       GDBusMethodInvocation          *invocation,
                       GVariant                       *result)
{
  g_dbus_method_invocation_return_value (invocation, result);

  if (self->current_search != NULL) {
    g_application_release (G_APPLICATION (self));

    pending_search_free (self->current_search);
    self->current_search = NULL;
  }
}

static void
cancel_current_search (NautilusShellSearchProviderApp *self)
{
  PendingSearch *search = self->current_search;

  if (search == NULL)
    return;

  nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (search->engine));
  finish_current_search (self, search->invocation, g_variant_new ("(as)", NULL));
}

static void
search_hits_added_cb (NautilusSearchEngine *engine,
                      GList                *hits,
                      gpointer              user_data)

{
  NautilusShellSearchProviderApp *self = user_data;
  PendingSearch *search = self->current_search;
  GList *l;
  NautilusSearchHit *hit;
  const gchar *hit_uri;

  g_debug ("*** Search engine hits added");

  for (l = hits; l != NULL; l = l->next) {
    hit = l->data;
    nautilus_search_hit_compute_scores (hit, search->query);
    hit_uri = nautilus_search_hit_get_uri (hit);
    g_debug ("    %s", hit_uri);

    g_hash_table_replace (search->hits, g_strdup (hit_uri), g_object_ref (hit));
  }
}

static void
search_hits_subtracted_cb (NautilusSearchEngine *engine,
                           GList                *hits,
                           gpointer              user_data)
{
  NautilusShellSearchProviderApp *self = user_data;
  PendingSearch *search = self->current_search;
  GList *l;
  NautilusSearchHit *hit;
  const gchar *hit_uri;

  g_debug ("*** Search engine hits subtracted");

  for (l = hits; l != NULL; l = l->next) {
    hit = l->data;
    hit_uri = nautilus_search_hit_get_uri (hit);
    g_debug ("    %s", hit_uri);

    g_hash_table_remove (search->hits, hit_uri);
  }
}

static gint
search_hit_compare_relevance (gconstpointer a,
                              gconstpointer b)
{
  NautilusSearchHit *hit_a, *hit_b;
  gdouble relevance_a, relevance_b;

  hit_a = NAUTILUS_SEARCH_HIT (a);
  hit_b = NAUTILUS_SEARCH_HIT (b);

  relevance_a = nautilus_search_hit_get_relevance (hit_a);
  relevance_b = nautilus_search_hit_get_relevance (hit_b);

  if (relevance_a > relevance_b)
    return -1;
  else if (relevance_a == relevance_b)
    return 0;

  return 1;
}

static void
search_finished_cb (NautilusSearchEngine *engine,
                    gpointer              user_data)
{
  NautilusShellSearchProviderApp *self = user_data;
  PendingSearch *search = self->current_search;
  GList *hits, *l;
  NautilusSearchHit *hit;
  GVariantBuilder builder;
  gint64 current_time;

  current_time = g_get_monotonic_time ();
  g_debug ("*** Search engine search finished - time elapsed %dms",
           (gint) ((current_time - search->start_time) / 1000));

  hits = g_hash_table_get_values (search->hits);
  hits = g_list_sort (hits, search_hit_compare_relevance);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (l = hits; l != NULL; l = l->next) {
    hit = l->data;
    g_variant_builder_add (&builder, "s", nautilus_search_hit_get_uri (hit));
  }

  g_list_free (hits);
  finish_current_search (self, search->invocation,
                         g_variant_new ("(as)", &builder));
}

static void
search_error_cb (NautilusSearchEngine *engine,
                 const gchar          *error_message,
                 gpointer              user_data)
{
  NautilusShellSearchProviderApp *self = user_data;
  PendingSearch *search = self->current_search;

  g_debug ("*** Search engine search error");
  finish_current_search (self, search->invocation, g_variant_new ("(as)", NULL));
}

static void
execute_search (NautilusShellSearchProviderApp *self,
                GDBusMethodInvocation          *invocation,
                gchar                         **terms)
{
  gchar *terms_joined, *home_uri;
  NautilusQuery *query;
  PendingSearch *pending_search;

  if (self->current_search != NULL)
    cancel_current_search (self);

  /* don't attempt searches for a single character */
  if (g_strv_length (terms) == 1 &&
      g_utf8_strlen (terms[0], -1) == 1) {
    finish_current_search (self, invocation,
                           g_variant_new ("(as)", NULL));
    return;
  }

  terms_joined = g_strjoinv (" ", terms);
  home_uri = nautilus_get_home_directory_uri ();

  query = nautilus_query_new ();
  nautilus_query_set_text (query, terms_joined);
  nautilus_query_set_location (query, home_uri);

  pending_search = g_slice_new0 (PendingSearch);
  pending_search->invocation = invocation;
  pending_search->hits = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  pending_search->query = query;
  pending_search->engine = nautilus_search_engine_new ();
  pending_search->start_time = g_get_monotonic_time ();

  g_signal_connect (pending_search->engine, "hits-added",
                    G_CALLBACK (search_hits_added_cb), self);
  g_signal_connect (pending_search->engine, "hits-subtracted",
                    G_CALLBACK (search_hits_subtracted_cb), self);
  g_signal_connect (pending_search->engine, "finished",
                    G_CALLBACK (search_finished_cb), self);
  g_signal_connect (pending_search->engine, "error",
                    G_CALLBACK (search_error_cb), self);

  self->current_search = pending_search;
  g_application_hold (G_APPLICATION (self));

  /* start searching */
  g_debug ("*** Search engine search started");
  nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (pending_search->engine),
                                      query);
  nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (pending_search->engine));

  g_free (home_uri);
  g_free (terms_joined);
}

static void
handle_get_initial_result_set (NautilusShellSearchProvider  *skeleton,
                               GDBusMethodInvocation        *invocation,
                               gchar                       **terms,
                               gpointer                      user_data)
{
  NautilusShellSearchProviderApp *self = user_data;

  g_debug ("****** GetInitialResultSet");
  execute_search (self, invocation, terms);
}

static void
handle_get_subsearch_result_set (NautilusShellSearchProvider  *skeleton,
                                 GDBusMethodInvocation        *invocation,
                                 gchar                       **previous_results,
                                 gchar                       **terms,
                                 gpointer                      user_data)
{
  NautilusShellSearchProviderApp *self = user_data;

  g_debug ("****** GetSubSearchResultSet");
  execute_search (self, invocation, terms);
}

typedef struct {
  NautilusShellSearchProviderApp *self;

  gint64 start_time;
  GDBusMethodInvocation *invocation;
} ResultMetasData;

static void
result_metas_data_free (ResultMetasData *data)
{
  g_clear_object (&data->self);
  g_slice_free (ResultMetasData, data);
}

static void
result_list_attributes_ready_cb (GList    *file_list,
                                 gpointer  user_data)
{
  ResultMetasData *data = user_data;
  GVariantBuilder builder, meta;
  NautilusFile *file;
  GList *l;
  gchar *uri, *display_name;
  GdkPixbuf *pix;
  gint64 current_time;
  gchar *thumbnail_path, *gicon_str;
  GIcon *gicon;
  GFile *location;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (l = file_list; l != NULL; l = l->next) {
    file = l->data;
    g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

    uri = nautilus_file_get_uri (file);
    display_name = nautilus_file_get_display_name (file);
    pix = nautilus_file_get_icon_pixbuf (file, 128, TRUE,
                                         NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

    g_variant_builder_add (&meta, "{sv}",
                           "id", g_variant_new_string (uri));
    g_variant_builder_add (&meta, "{sv}",
                           "name", g_variant_new_string (display_name));

    gicon = NULL;
    thumbnail_path = nautilus_file_get_thumbnail_path (file);

    if (thumbnail_path != NULL) {
      location = g_file_new_for_path (thumbnail_path);
      gicon = g_file_icon_new (location);

      g_free (thumbnail_path);
      g_object_unref (location);
    } else {
      gicon = nautilus_file_get_gicon (file, 0);
    }

    if (gicon != NULL) {
      gicon_str = g_icon_to_string (gicon);
      g_variant_builder_add (&meta, "{sv}",
                             "gicon", g_variant_new_string (gicon_str));

      g_free (gicon_str);
      g_object_unref (gicon);
    } else {
      pix = nautilus_file_get_icon_pixbuf (file, 128, TRUE,
                                           NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);

      g_variant_builder_add (&meta, "{sv}",
                             "icon-data", variant_from_pixbuf (pix));
      g_object_unref (pix);
    }

    g_variant_builder_add (&builder, "a{sv}", &meta);

    g_free (display_name);
    g_free (uri);
  }

  current_time = g_get_monotonic_time ();
  g_debug ("*** GetResultMetas completed - time elapsed %dms",
           (gint) ((current_time - data->start_time) / 1000));

  g_dbus_method_invocation_return_value (data->invocation,
                                         g_variant_new ("(aa{sv})", &builder));
  result_metas_data_free (data);
}

static void
handle_get_result_metas (NautilusShellSearchProvider  *skeleton,
                         GDBusMethodInvocation        *invocation,
                         gchar                       **results,
                         gpointer                      user_data)
{
  NautilusShellSearchProviderApp *self = user_data;
  GList *nautilus_files = NULL;
  const gchar *uri;
  ResultMetasData *data;
  gint idx;

  g_debug ("****** GetResultMetas");

  for (idx = 0; results[idx] != NULL; idx++) {
    uri = results[idx];
    nautilus_files = g_list_prepend (nautilus_files, nautilus_file_get_by_uri (uri));
  }

  data = g_slice_new0 (ResultMetasData);
  data->self = g_object_ref (self);
  data->invocation = invocation;
  data->start_time = g_get_monotonic_time ();

  nautilus_file_list_call_when_ready (nautilus_files,
                                      NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                      NULL,
                                      result_list_attributes_ready_cb,
                                      data);
  nautilus_file_list_free (nautilus_files);
}

static void
handle_activate_result (NautilusShellSearchProvider *skeleton,
                        GDBusMethodInvocation       *invocation,
                        gchar                       *result,
                        gpointer                     user_data)
{
  GError *error = NULL;

  gtk_show_uri (NULL, result, GDK_CURRENT_TIME, &error);
  if (error != NULL) {
    g_warning ("Unable to activate %s: %s", result, error->message);
    g_error_free (error);
  }
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
  cancel_current_search (self);

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

  gtk_init (&argc, &argv);

  app = nautilus_shell_search_provider_app_new ();
  res = g_application_run (app, argc, argv);
  g_object_unref (app);

  return res;
}

