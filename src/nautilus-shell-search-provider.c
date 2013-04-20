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
#include <string.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-search-engine.h>
#include <libnautilus-private/nautilus-search-provider.h>
#include <libnautilus-private/nautilus-ui-utilities.h>

#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-shell-search-provider-generated.h"
#include "nautilus-shell-search-provider.h"

typedef struct {
  NautilusShellSearchProvider *self;

  NautilusSearchEngine *engine;
  NautilusQuery *query;

  GHashTable *hits;
  GDBusMethodInvocation *invocation;

  gint64 start_time;
} PendingSearch;

struct _NautilusShellSearchProvider {
  GObject parent;

  guint name_owner_id;
  GDBusObjectManagerServer *object_manager;
  NautilusShellSearchProvider2 *skeleton;

  PendingSearch *current_search;

  GHashTable *metas_cache;

  NautilusBookmarkList *bookmarks;
  GVolumeMonitor *volumes;
};

G_DEFINE_TYPE (NautilusShellSearchProvider, nautilus_shell_search_provider, G_TYPE_OBJECT)

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

static gchar *
get_display_name (NautilusShellSearchProvider *self,
                  NautilusFile                   *file)
{
  GFile *location;
  NautilusBookmark *bookmark;

  location = nautilus_file_get_location (file);
  bookmark = nautilus_bookmark_list_item_with_location (self->bookmarks, location, NULL);
  g_object_unref (location);

  if (bookmark)
    return g_strdup (nautilus_bookmark_get_name (bookmark));
  else
    return nautilus_file_get_display_name (file);
}

static GIcon *
get_gicon (NautilusShellSearchProvider *self,
           NautilusFile                   *file)
{
  GFile *location;
  NautilusBookmark *bookmark;

  location = nautilus_file_get_location (file);
  bookmark = nautilus_bookmark_list_item_with_location (self->bookmarks, location, NULL);
  g_object_unref (location);

  if (bookmark)
    return nautilus_bookmark_get_icon (bookmark);
  else
    return nautilus_file_get_gicon (file, 0);
}

static void
pending_search_free (PendingSearch *search)
{
  g_hash_table_destroy (search->hits);
  g_clear_object (&search->query);
  g_clear_object (&search->engine);
  g_clear_object (&search->invocation);

  g_slice_free (PendingSearch, search);
}

static void
pending_search_finish (PendingSearch         *search,
                       GDBusMethodInvocation *invocation,
                       GVariant              *result)
{
  NautilusShellSearchProvider *self = search->self;

  g_dbus_method_invocation_return_value (invocation, result);

  if (search == self->current_search)
    self->current_search = NULL;

  g_application_release (g_application_get_default ());
  pending_search_free (search);
}

static void
cancel_current_search (NautilusShellSearchProvider *self)
{
  if (self->current_search != NULL)
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (self->current_search->engine));
}

static void
search_hits_added_cb (NautilusSearchEngine *engine,
                      GList                *hits,
                      gpointer              user_data)

{
  PendingSearch *search = user_data;
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
  PendingSearch *search = user_data;
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
  pending_search_finish (search, search->invocation,
                         g_variant_new ("(as)", &builder));
}

static void
search_error_cb (NautilusSearchEngine *engine,
                 const gchar          *error_message,
                 gpointer              user_data)
{
  NautilusShellSearchProvider *self = user_data;
  PendingSearch *search = self->current_search;

  g_debug ("*** Search engine search error");
  pending_search_finish (search, search->invocation,
                         g_variant_new ("(as)", NULL));
}

typedef struct {
  gchar *uri;
  gchar *string_for_compare;
} SearchHitCandidate;

static void
search_hit_candidate_free (SearchHitCandidate *candidate)
{
  g_free (candidate->uri);
  g_free (candidate->string_for_compare);

  g_slice_free (SearchHitCandidate, candidate);
}

static SearchHitCandidate *
search_hit_candidate_new (const gchar *uri,
                          const gchar *name)
{
  SearchHitCandidate *candidate = g_slice_new0 (SearchHitCandidate);

  candidate->uri = g_strdup (uri);
  candidate->string_for_compare = g_strdup (name);

  return candidate;
}

static void
search_add_volumes_and_bookmarks (PendingSearch *search)
{
  NautilusSearchHit *hit;
  NautilusBookmark *bookmark;
  const gchar *name;
  gint length, idx;
  gchar *string, *uri;
  gdouble match;
  GList *l, *m, *drives, *volumes, *mounts, *mounts_to_check, *candidates;
  GDrive *drive;
  GVolume *volume;
  GMount *mount;
  GFile *location;
  SearchHitCandidate *candidate;

  candidates = NULL;

  /* first add bookmarks */
  length = nautilus_bookmark_list_length (search->self->bookmarks);
  for (idx = 0; idx < length; idx++) {
    bookmark = nautilus_bookmark_list_item_at (search->self->bookmarks, idx);

    name = nautilus_bookmark_get_name (bookmark);
    if (name == NULL)
      continue;

    uri = nautilus_bookmark_get_uri (bookmark);
    candidate = search_hit_candidate_new (uri, name);
    candidates = g_list_prepend (candidates, candidate);

    g_free (uri);
  }

  /* home dir */
  uri = nautilus_get_home_directory_uri ();
  candidate = search_hit_candidate_new (uri, _("Home"));
  candidates = g_list_prepend (candidates, candidate);
  g_free (uri);

  /* trash */
  candidate = search_hit_candidate_new ("trash:///", _("Trash"));
  candidates = g_list_prepend (candidates, candidate);

  /* now add mounts */
  mounts_to_check = NULL;

  /* first check all connected drives */
  drives = g_volume_monitor_get_connected_drives (search->self->volumes);
  for (l = drives; l != NULL; l = l->next) {
    drive = l->data;
    volumes = g_drive_get_volumes (drive);

    for (m = volumes; m != NULL; m = m->next) {
      volume = m->data;
      mount = g_volume_get_mount (volume);
      if (mount != NULL) {
        mounts_to_check = g_list_prepend (mounts_to_check, mount);
      }
    }

    g_list_free_full (volumes, g_object_unref);
  }
  g_list_free_full (drives, g_object_unref);

  /* then volumes that don't have a drive */
  volumes = g_volume_monitor_get_volumes (search->self->volumes);
  for (l = volumes; l != NULL; l = l->next) {
    volume = l->data;
    drive = g_volume_get_drive (volume);

    if (drive == NULL) {
      mount = g_volume_get_mount (volume);
      if (mount != NULL) {
        mounts_to_check = g_list_prepend (mounts_to_check, mount);
      }
    }
    g_clear_object (&drive);
  }
  g_list_free_full (volumes, g_object_unref);

  /* then mounts that have no volume */
  mounts = g_volume_monitor_get_mounts (search->self->volumes);
  for (l = mounts; l != NULL; l = l->next) {
    mount = l->data;

    if (g_mount_is_shadowed (mount))
      continue;

    volume = g_mount_get_volume (mount);
    if (volume == NULL)
      mounts_to_check = g_list_prepend (mounts_to_check, g_object_ref (mount));
    g_clear_object (&volume);
  }
  g_list_free_full (mounts, g_object_unref);

  /* actually add mounts to candidates */
  for (l = mounts_to_check; l != NULL; l = l->next) {
    mount = l->data;

    string = g_mount_get_name (mount);
    if (string == NULL)
      continue;

    location = g_mount_get_default_location (mount);
    uri = g_file_get_uri (location);
    candidate = search_hit_candidate_new (uri, string);
    candidates = g_list_prepend (candidates, candidate);

    g_free (uri);
    g_free (string);
    g_object_unref (location);
  }
  g_list_free_full (mounts_to_check, g_object_unref);

  /* now do the actual string matching */
  candidates = g_list_reverse (candidates);

  for (l = candidates; l != NULL; l = l->next) {
    candidate = l->data;
    match = nautilus_query_matches_string (search->query,
                                           candidate->string_for_compare);

    if (match > -1) {
      hit = nautilus_search_hit_new (candidate->uri);
      nautilus_search_hit_set_fts_rank (hit, match);
      nautilus_search_hit_compute_scores (hit, search->query);
      g_hash_table_replace (search->hits, g_strdup (candidate->uri), hit);
    }
  }
  g_list_free_full (candidates, (GDestroyNotify) search_hit_candidate_free);
}

static void
execute_search (NautilusShellSearchProvider *self,
                GDBusMethodInvocation          *invocation,
                gchar                         **terms)
{
  gchar *terms_joined, *home_uri;
  NautilusQuery *query;
  PendingSearch *pending_search;

  cancel_current_search (self);

  /* don't attempt searches for a single character */
  if (g_strv_length (terms) == 1 &&
      g_utf8_strlen (terms[0], -1) == 1) {
    g_dbus_method_invocation_return_value (invocation, g_variant_new ("(as)", NULL));
    return;
  }

  terms_joined = g_strjoinv (" ", terms);
  home_uri = nautilus_get_home_directory_uri ();

  query = nautilus_query_new ();
  nautilus_query_set_show_hidden_files (query, FALSE);
  nautilus_query_set_text (query, terms_joined);
  nautilus_query_set_location (query, home_uri);

  pending_search = g_slice_new0 (PendingSearch);
  pending_search->invocation = g_object_ref (invocation);
  pending_search->hits = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  pending_search->query = query;
  pending_search->engine = nautilus_search_engine_new ();
  pending_search->start_time = g_get_monotonic_time ();
  pending_search->self = self;

  g_signal_connect (pending_search->engine, "hits-added",
                    G_CALLBACK (search_hits_added_cb), pending_search);
  g_signal_connect (pending_search->engine, "finished",
                    G_CALLBACK (search_finished_cb), pending_search);
  g_signal_connect (pending_search->engine, "error",
                    G_CALLBACK (search_error_cb), pending_search);

  self->current_search = pending_search;
  g_application_hold (g_application_get_default ());

  search_add_volumes_and_bookmarks (pending_search);

  /* start searching */
  g_debug ("*** Search engine search started");
  nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (pending_search->engine),
                                      query);
  nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (pending_search->engine));

  g_free (home_uri);
  g_free (terms_joined);
}

static void
handle_get_initial_result_set (NautilusShellSearchProvider2  *skeleton,
                               GDBusMethodInvocation         *invocation,
                               gchar                        **terms,
                               gpointer                       user_data)
{
  NautilusShellSearchProvider *self = user_data;

  g_debug ("****** GetInitialResultSet");
  execute_search (self, invocation, terms);
}

static void
handle_get_subsearch_result_set (NautilusShellSearchProvider2  *skeleton,
                                 GDBusMethodInvocation         *invocation,
                                 gchar                        **previous_results,
                                 gchar                        **terms,
                                 gpointer                       user_data)
{
  NautilusShellSearchProvider *self = user_data;

  g_debug ("****** GetSubSearchResultSet");
  execute_search (self, invocation, terms);
}

typedef struct {
  NautilusShellSearchProvider *self;

  gint64 start_time;
  GDBusMethodInvocation *invocation;

  gchar **uris;
} ResultMetasData;

static void
result_metas_data_free (ResultMetasData *data)
{
  g_clear_object (&data->self);
  g_clear_object (&data->invocation);
  g_strfreev (data->uris);

  g_slice_free (ResultMetasData, data);
}

static void
result_metas_return_from_cache (ResultMetasData *data)
{
  GVariantBuilder builder;
  GVariant *meta;
  gint64 current_time;
  gint idx;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (idx = 0; data->uris[idx] != NULL; idx++) {
    meta = g_hash_table_lookup (data->self->metas_cache,
                                data->uris[idx]);
    g_variant_builder_add_value (&builder, meta);
  }

  current_time = g_get_monotonic_time ();
  g_debug ("*** GetResultMetas completed - time elapsed %dms",
           (gint) ((current_time - data->start_time) / 1000));

  g_dbus_method_invocation_return_value (data->invocation,
                                         g_variant_new ("(aa{sv})", &builder));
}

static void
result_list_attributes_ready_cb (GList    *file_list,
                                 gpointer  user_data)
{
  ResultMetasData *data = user_data;
  GVariantBuilder meta;
  NautilusFile *file;
  GList *l;
  gchar *uri, *display_name;
  GdkPixbuf *pix;
  gchar *thumbnail_path, *gicon_str;
  GIcon *gicon;
  GFile *location;
  GVariant *meta_variant;

  for (l = file_list; l != NULL; l = l->next) {
    file = l->data;
    g_variant_builder_init (&meta, G_VARIANT_TYPE ("a{sv}"));

    uri = nautilus_file_get_uri (file);
    display_name = get_display_name (data->self, file);

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
      gicon = get_gicon (data->self, file);
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

    meta_variant = g_variant_builder_end (&meta);
    g_hash_table_insert (data->self->metas_cache,
                         g_strdup (uri), g_variant_ref_sink (meta_variant));

    g_free (display_name);
    g_free (uri);
  }

  result_metas_return_from_cache (data);
  result_metas_data_free (data);
}

static void
handle_get_result_metas (NautilusShellSearchProvider2  *skeleton,
                         GDBusMethodInvocation         *invocation,
                         gchar                        **results,
                         gpointer                       user_data)
{
  NautilusShellSearchProvider *self = user_data;
  GList *missing_files = NULL;
  const gchar *uri;
  ResultMetasData *data;
  gint idx;

  g_debug ("****** GetResultMetas");

  for (idx = 0; results[idx] != NULL; idx++) {
    uri = results[idx];

    if (!g_hash_table_lookup (self->metas_cache, uri)) {
      missing_files = g_list_prepend (missing_files, nautilus_file_get_by_uri (uri));
    }
  }

  data = g_slice_new0 (ResultMetasData);
  data->self = g_object_ref (self);
  data->invocation = g_object_ref (invocation);
  data->start_time = g_get_monotonic_time ();
  data->uris = g_strdupv (results);

  if (missing_files == NULL) {
    result_metas_return_from_cache (data);
    result_metas_data_free (data);
    return;
  }

  nautilus_file_list_call_when_ready (missing_files,
                                      NAUTILUS_FILE_ATTRIBUTES_FOR_ICON,
                                      NULL,
                                      result_list_attributes_ready_cb,
                                      data);
  nautilus_file_list_free (missing_files);
}

static void
handle_activate_result (NautilusShellSearchProvider2 *skeleton,
                        GDBusMethodInvocation        *invocation,
                        gchar                        *result,
                        gchar                       **terms,
                        guint32                       timestamp,
                        gpointer                      user_data)
{
  gboolean res;
  GFile *file;

  res = gtk_show_uri (NULL, result, timestamp, NULL);

  if (!res) {
    file = g_file_new_for_uri (result);
    g_application_open (g_application_get_default (), &file, 1, "");
    g_object_unref (file);
  }

  nautilus_shell_search_provider2_complete_activate_result (skeleton, invocation);
}

static void
handle_launch_search (NautilusShellSearchProvider2 *skeleton,
                      GDBusMethodInvocation        *invocation,
                      gchar                       **terms,
                      guint32                       timestamp,
                      gpointer                      user_data)
{
  GApplication *app = g_application_get_default ();
  gchar *string = g_strjoinv (" ", terms);
  gchar *uri = nautilus_get_home_directory_uri ();

  g_action_group_activate_action (G_ACTION_GROUP (app), "search",
                                  g_variant_new ("(ss)", uri, string));

  g_free (string);
  g_free (uri);

  nautilus_shell_search_provider2_complete_launch_search (skeleton, invocation);
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
  NautilusShellSearchProvider *self = user_data;

  self->object_manager = g_dbus_object_manager_server_new ("/org/gnome/Nautilus/SearchProvider");
  self->skeleton = nautilus_shell_search_provider2_skeleton_new ();

  g_signal_connect (self->skeleton, "handle-get-initial-result-set",
                    G_CALLBACK (handle_get_initial_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-subsearch-result-set",
                    G_CALLBACK (handle_get_subsearch_result_set), self);
  g_signal_connect (self->skeleton, "handle-get-result-metas",
                    G_CALLBACK (handle_get_result_metas), self);
  g_signal_connect (self->skeleton, "handle-activate-result",
                    G_CALLBACK (handle_activate_result), self);
  g_signal_connect (self->skeleton, "handle-launch-search",
                    G_CALLBACK (handle_launch_search), self);

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->skeleton),
                                    connection,
                                    "/org/gnome/Nautilus/SearchProvider", NULL);
  g_dbus_object_manager_server_set_connection (self->object_manager, connection);

  g_application_release (g_application_get_default ());
}

static void
search_provider_dispose (GObject *obj)
{
  NautilusShellSearchProvider *self = NAUTILUS_SHELL_SEARCH_PROVIDER (obj);

  if (self->name_owner_id != 0) {
    g_bus_unown_name (self->name_owner_id);
    self->name_owner_id = 0;
  }

  if (self->skeleton != NULL) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->skeleton));
    g_clear_object (&self->skeleton);
  }

  g_clear_object (&self->object_manager);
  g_hash_table_destroy (self->metas_cache);
  cancel_current_search (self);

  g_clear_object (&self->volumes);

  G_OBJECT_CLASS (nautilus_shell_search_provider_parent_class)->dispose (obj);
}

static void
nautilus_shell_search_provider_init (NautilusShellSearchProvider *self)
{
  self->metas_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                             g_free, (GDestroyNotify) g_variant_unref);
  self->bookmarks = nautilus_application_get_bookmarks (NAUTILUS_APPLICATION (g_application_get_default ()));
  self->volumes = g_volume_monitor_get ();

  g_application_hold (g_application_get_default ());
  self->name_owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                                        "org.gnome.Nautilus.SearchProvider",
                                        G_BUS_NAME_OWNER_FLAGS_NONE,
                                        search_provider_bus_acquired_cb,
                                        search_provider_name_acquired_cb,
                                        search_provider_name_lost_cb,
                                        self, NULL);
}

static void
nautilus_shell_search_provider_class_init (NautilusShellSearchProviderClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = search_provider_dispose;
}

NautilusShellSearchProvider *
nautilus_shell_search_provider_new (void)
{
  return g_object_new (nautilus_shell_search_provider_get_type (),
                       NULL);
}
