/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nemo-bookmark-list.c - implementation of centralized list of bookmarks.
 */

#include <config.h>
#include "nemo-bookmark-list.h"

#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-icon-names.h>
#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-desktop-utils.h>

#include <gio/gio.h>
#include <string.h>

#define MAX_BOOKMARK_LENGTH 80
#define LOAD_JOB 1
#define SAVE_JOB 2

#define KEY_ICON_URI    "IconUri"
#define KEY_ICON_NAME    "IconName"
#define KEY_ICON_EMBLEMS "IconEmblems"

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };
static char *window_geometry;
static NemoBookmarkList *singleton = NULL;

/* forward declarations */

static void        nemo_bookmark_list_load_file     (NemoBookmarkList *bookmarks);
static void        nemo_bookmark_list_save_file     (NemoBookmarkList *bookmarks);

G_DEFINE_TYPE(NemoBookmarkList, nemo_bookmark_list, G_TYPE_OBJECT);

static void
ensure_proper_file_permissions (GFile *file)
{
    if (geteuid () == 0) {
        struct passwd *pwent;
        pwent = gnome_desktop_get_session_user_pwent ();

        gchar *path = g_file_get_path (file);

        if (g_strcmp0 (pwent->pw_dir, g_get_home_dir ()) == 0) {
            G_GNUC_UNUSED int res;

            res = chown (path, pwent->pw_uid, pwent->pw_gid);
        }

        g_free (path);
    }
}

static NemoBookmark *
new_bookmark_from_uri (const char *uri, const char *label, NemoBookmarkMetadata *md)
{
	NemoBookmark *new_bookmark;
	GFile *location;

	location = NULL;
	if (uri) {
		location = g_file_new_for_uri (uri);
	}
	
	new_bookmark = NULL;

	if (location) {
		new_bookmark = nemo_bookmark_new (location, label, NULL, md);
		g_object_unref (location);
	}

	return new_bookmark;
}

static GFile *
nemo_bookmark_list_get_legacy_file (void)
{
	char *filename;
	GFile *file;

	filename = g_build_filename (g_get_home_dir (),
				     ".gtk-bookmarks",
				     NULL);
	file = g_file_new_for_path (filename);

	g_free (filename);

	return file;
}

static GFile *
nemo_bookmark_list_get_file (void)
{
    char *filename;
    GFile *file;

    filename = g_build_filename (g_get_user_config_dir (),
                                 "gtk-3.0",
                                 "bookmarks",
                                 NULL);
    file = g_file_new_for_path (filename);

    g_free (filename);

    return file;
}

/* Initialization.  */

static void
bookmark_in_list_changed_callback (NemoBookmark     *bookmark,
				   NemoBookmarkList *bookmarks)
{
	g_assert (NEMO_IS_BOOKMARK (bookmark));
	g_assert (NEMO_IS_BOOKMARK_LIST (bookmarks));
	/* save changes to the list */
	nemo_bookmark_list_save_file (bookmarks);
}

static gboolean
idle_notify (NemoBookmarkList *bookmarks)
{
    g_signal_emit (bookmarks, signals[CHANGED], 0);

    bookmarks->idle_notify_id = 0;
    return FALSE;
}

static void
bookmark_in_list_notify (GObject *object,
			 GParamSpec *pspec,
			 NemoBookmarkList *bookmarks)
{
	/* emit the changed signal without saving, as only appearance properties changed */

    if (bookmarks->idle_notify_id > 0) {
        g_source_remove (bookmarks->idle_notify_id);
        bookmarks->idle_notify_id = 0;
    }

    bookmarks->idle_notify_id = g_idle_add ((GSourceFunc) idle_notify, bookmarks);
}

static gboolean
bookmark_location_mounted_callback (NemoBookmark *bookmark,
                                    GFile *location,
                                    NemoBookmarkList *bookmarks)
{
    gboolean ret = FALSE;

    GList *volumes = g_volume_monitor_get_mounts (bookmarks->volume_monitor);
    GList *iter = volumes;

    while (iter != NULL) {
        GMount *mount = G_MOUNT (iter->data);
        GFile *mount_location = g_mount_get_root (mount);

        gchar *mount_root_uri = g_file_get_uri (mount_location);
        gchar *location_uri = g_file_get_uri (location);

        ret = g_str_has_prefix (location_uri, mount_root_uri);

        g_free (mount_root_uri);
        g_free (location_uri);

        g_object_unref (mount_location);

        if (ret == TRUE)
            break;

        iter = iter->next;
    }

    g_list_free_full (volumes, (GDestroyNotify) g_object_unref);

    return ret;
}

static void
stop_monitoring_bookmark (NemoBookmarkList *bookmarks,
			  NemoBookmark     *bookmark)
{
	g_signal_handlers_disconnect_by_func (bookmark,
					      bookmark_in_list_changed_callback,
					      bookmarks);
    g_signal_handlers_disconnect_by_func (bookmark,
                          bookmark_location_mounted_callback,
                          bookmarks);
}

static void
stop_monitoring_one (gpointer data, gpointer user_data)
{
	g_assert (NEMO_IS_BOOKMARK (data));
	g_assert (NEMO_IS_BOOKMARK_LIST (user_data));

	stop_monitoring_bookmark (NEMO_BOOKMARK_LIST (user_data), 
				  NEMO_BOOKMARK (data));
}

static void
clear_bookmarks (NemoBookmarkList *bookmarks, GList *list)
{
	g_list_foreach (list, stop_monitoring_one, bookmarks);
	g_list_free_full (list, g_object_unref);
	list = NULL;
}

static void
do_finalize (GObject *object)
{
	if (NEMO_BOOKMARK_LIST (object)->monitor != NULL) {
		g_file_monitor_cancel (NEMO_BOOKMARK_LIST (object)->monitor);
		NEMO_BOOKMARK_LIST (object)->monitor = NULL;
	}

	g_queue_free (NEMO_BOOKMARK_LIST (object)->pending_ops);

	clear_bookmarks (NEMO_BOOKMARK_LIST (object), NEMO_BOOKMARK_LIST (object)->list);

    g_object_unref (NEMO_BOOKMARK_LIST (object)->volume_monitor);

	G_OBJECT_CLASS (nemo_bookmark_list_parent_class)->finalize (object);
}

static GObject *
do_constructor (GType type,
                guint n_construct_params,
                GObjectConstructParam *construct_params)
{
	GObject *retval;

	if (singleton != NULL) {
		return g_object_ref (singleton);
	}

	retval = G_OBJECT_CLASS (nemo_bookmark_list_parent_class)->constructor
		(type, n_construct_params, construct_params);

	singleton = NEMO_BOOKMARK_LIST (retval);
	g_object_add_weak_pointer (retval, (gpointer) &singleton);

	return retval;
}

static void
do_constructed (GObject *object)
{
    G_OBJECT_CLASS (nemo_bookmark_list_parent_class)->constructed (object);

    nemo_bookmark_list_load_file (NEMO_BOOKMARK_LIST (object));
}

static void
nemo_bookmark_list_class_init (NemoBookmarkListClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = do_finalize;
	object_class->constructor = do_constructor;
    object_class->constructed = do_constructed;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NemoBookmarkListClass, 
					       changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

static void
bookmark_monitor_changed_cb (GFileMonitor      *monitor,
			     GFile             *child,
			     GFile             *other_file,
			     GFileMonitorEvent  eflags,
			     gpointer           user_data)
{
	if (eflags == G_FILE_MONITOR_EVENT_CHANGED ||
	    eflags == G_FILE_MONITOR_EVENT_CREATED) {
		g_return_if_fail (NEMO_IS_BOOKMARK_LIST (NEMO_BOOKMARK_LIST (user_data)));
		nemo_bookmark_list_load_file (NEMO_BOOKMARK_LIST (user_data));
	}
}

static void
volume_monitor_activity_cb (GVolumeMonitor *monitor, GMount *mount, gpointer user_data)
{
    NemoBookmarkList *bookmarks = NEMO_BOOKMARK_LIST (user_data);

    nemo_bookmark_list_load_file (bookmarks);
}

static void
nemo_bookmark_list_init (NemoBookmarkList *bookmarks)
{
	GFile *file;

    bookmarks->list = NULL;
    bookmarks->idle_notify_id = 0;

	bookmarks->pending_ops = g_queue_new ();

	file = nemo_bookmark_list_get_file ();
	bookmarks->monitor = g_file_monitor_file (file, 0, NULL, NULL);
	g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);

	g_signal_connect (bookmarks->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb),
                      bookmarks);

	g_object_unref (file);

    bookmarks->volume_monitor = g_volume_monitor_get ();

    g_signal_connect (bookmarks->volume_monitor, "mount-added",
                      G_CALLBACK (volume_monitor_activity_cb), bookmarks);
    g_signal_connect (bookmarks->volume_monitor, "mount-removed",
                      G_CALLBACK (volume_monitor_activity_cb), bookmarks);
    g_signal_connect (bookmarks->volume_monitor, "mount-changed",
                      G_CALLBACK (volume_monitor_activity_cb), bookmarks);
}

static void
connect_bookmark_signals (NemoBookmark     *bookmark,
                          NemoBookmarkList *bookmarks)
{
    g_signal_connect_object (bookmark, "location-mounted",
                 G_CALLBACK (bookmark_location_mounted_callback), bookmarks, 0);
    g_signal_connect_object (bookmark, "contents-changed",
                 G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
    g_signal_connect_object (bookmark, "notify::icon",
                 G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);
    g_signal_connect_object (bookmark, "notify::name",
                 G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);

    nemo_bookmark_connect (bookmark);
}

static void
insert_bookmark_internal (NemoBookmarkList *bookmarks,
			  NemoBookmark     *bookmark,
			  int                   index)
{
	bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

    connect_bookmark_signals (bookmark, bookmarks);
}

/**
 * nemo_bookmark_list_append:
 *
 * Append a bookmark to a bookmark list.
 * @bookmarks: NemoBookmarkList to append to.
 * @bookmark: Bookmark to append a copy of.
 **/
void
nemo_bookmark_list_append (NemoBookmarkList *bookmarks, 
			       NemoBookmark     *bookmark)
{
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (NEMO_IS_BOOKMARK (bookmark));

	insert_bookmark_internal (bookmarks, 
				  nemo_bookmark_copy (bookmark), 
				  -1);

	nemo_bookmark_list_save_file (bookmarks);
}

/**
 * nemo_bookmark_list_contains:
 *
 * Check whether a bookmark with matching name and url is already in the list.
 * @bookmarks: NemoBookmarkList to check contents of.
 * @bookmark: NemoBookmark to match against.
 * 
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
nemo_bookmark_list_contains (NemoBookmarkList *bookmarks, 
				 NemoBookmark     *bookmark)
{
	g_return_val_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks), FALSE);
	g_return_val_if_fail (NEMO_IS_BOOKMARK (bookmark), FALSE);

	return g_list_find_custom (bookmarks->list,
				   (gpointer)bookmark, 
				   nemo_bookmark_compare_with) 
		!= NULL;
}

/**
 * nemo_bookmark_list_delete_item_at:
 * 
 * Delete the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 **/
void
nemo_bookmark_list_delete_item_at (NemoBookmarkList *bookmarks, 
				       guint                 index)
{
	GList *doomed;

	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (index < g_list_length (bookmarks->list));

	doomed = g_list_nth (bookmarks->list, index);
	g_return_if_fail (doomed != NULL);

	bookmarks->list = g_list_remove_link (bookmarks->list, doomed);

	g_assert (NEMO_IS_BOOKMARK (doomed->data));
	stop_monitoring_bookmark (bookmarks, NEMO_BOOKMARK (doomed->data));
	g_object_unref (doomed->data);

	g_list_free_1 (doomed);

	nemo_bookmark_list_save_file (bookmarks);
}

/**
 * nemo_bookmark_list_move_item:
 *
 * Move the item from the given position to the destination.
 * @index: the index of the first bookmark.
 * @destination: the index of the second bookmark.
 **/
void
nemo_bookmark_list_move_item (NemoBookmarkList *bookmarks,
				  guint index,
				  guint destination)
{
	GList *bookmark_item;

	if (index == destination) {
		return;
	}

	bookmark_item = g_list_nth (bookmarks->list, index);
	g_return_if_fail (bookmark_item != NULL);

	bookmarks->list = g_list_remove_link (bookmarks->list,
					      bookmark_item);

	if (index < destination) {
		bookmarks->list = g_list_insert (bookmarks->list,
						 bookmark_item->data,
						 destination - 1);
	} else {
		bookmarks->list = g_list_insert (bookmarks->list,
						 bookmark_item->data,
						 destination);
	}

	nemo_bookmark_list_save_file (bookmarks);
}

static gint
nemo_bookmark_list_compare_func (gconstpointer a, gconstpointer b)
{
    g_assert (NEMO_IS_BOOKMARK (a));
    g_assert (NEMO_IS_BOOKMARK (b));

    return g_utf8_collate (nemo_bookmark_get_name (NEMO_BOOKMARK (a)),
                           nemo_bookmark_get_name (NEMO_BOOKMARK (b)));
}

/**
 * nemo_bookmark_list_sort_ascending:
 *
 * Sort bookmarks in ascending order.
 * @bookmarks: the list of bookmarks.
 **/
void
nemo_bookmark_list_sort_ascending (NemoBookmarkList *bookmarks)
{
    g_assert (NEMO_IS_BOOKMARK_LIST (bookmarks));

    bookmarks->list = g_list_sort (
        bookmarks->list, (GCompareFunc)nemo_bookmark_list_compare_func);

    /* Save bookmarks to file. This will also inform widgets about the changes
     * we just made to the list.
     */
    nemo_bookmark_list_save_file (bookmarks);
}

/**
 * nemo_bookmark_list_delete_items_with_uri:
 * 
 * Delete all bookmarks with the given uri.
 * @bookmarks: the list of bookmarks.
 * @uri: The uri to match.
 **/
void
nemo_bookmark_list_delete_items_with_uri (NemoBookmarkList *bookmarks, 
				      	      const char           *uri)
{
	GList *node, *next;
	gboolean list_changed;
	char *bookmark_uri;

	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (uri != NULL);

	list_changed = FALSE;
	for (node = bookmarks->list; node != NULL;  node = next) {
		next = node->next;

		bookmark_uri = nemo_bookmark_get_uri (NEMO_BOOKMARK (node->data));
		if (g_strcmp0 (bookmark_uri, uri) == 0) {
			bookmarks->list = g_list_remove_link (bookmarks->list, node);
			stop_monitoring_bookmark (bookmarks, NEMO_BOOKMARK (node->data));
			g_object_unref (node->data);
			g_list_free_1 (node);
			list_changed = TRUE;
		}
		g_free (bookmark_uri);
	}

	if (list_changed) {
		nemo_bookmark_list_save_file (bookmarks);
	}
}

/**
 * nemo_bookmark_list_get_window_geometry:
 * 
 * Get a string representing the bookmark_list's window's geometry.
 * This is the value set earlier by nemo_bookmark_list_set_window_geometry.
 * @bookmarks: the list of bookmarks associated with the window.
 * Return value: string representation of window's geometry, suitable for
 * passing to gnome_parse_geometry(), or NULL if
 * no window geometry has yet been saved for this bookmark list.
 **/
const char *
nemo_bookmark_list_get_window_geometry (NemoBookmarkList *bookmarks)
{
	return window_geometry;
}

/**
 * nemo_bookmark_list_insert_item:
 * 
 * Insert a bookmark at a specified position.
 * @bookmarks: the list of bookmarks.
 * @index: the position to insert the bookmark at.
 * @new_bookmark: the bookmark to insert a copy of.
 **/
void
nemo_bookmark_list_insert_item (NemoBookmarkList *bookmarks,
				    NemoBookmark     *new_bookmark,
				    guint                 index)
{
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (index <= g_list_length (bookmarks->list));

	insert_bookmark_internal (bookmarks,
				  nemo_bookmark_copy (new_bookmark), 
				  index);

	nemo_bookmark_list_save_file (bookmarks);
}

GList *
nemo_bookmark_list_get_for_uri (NemoBookmarkList   *bookmarks,
                                      const char   *uri)
{
    g_return_val_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks), NULL);

    GList *iter;
    GList *results = NULL;
    NemoBookmark *bookmark;

    for (iter = bookmarks->list; iter != NULL; iter = iter->next) {
        bookmark = iter->data;
        gchar *bm_uri = nemo_bookmark_get_uri (bookmark);
        if (g_strcmp0 (uri, bm_uri) == 0) {
            results = g_list_append (results, bookmark);
        }
        g_free (bm_uri);
    }

    return results;
}

/**
 * nemo_bookmark_list_item_at:
 * 
 * Get the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 * 
 * Return value: the bookmark at position @index in @bookmarks.
 **/
NemoBookmark *
nemo_bookmark_list_item_at (NemoBookmarkList *bookmarks, guint index)
{
	g_return_val_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks), NULL);
	g_return_val_if_fail (index < g_list_length (bookmarks->list), NULL);

	return NEMO_BOOKMARK (g_list_nth_data (bookmarks->list, index));
}

/**
 * nemo_bookmark_list_length:
 * 
 * Get the number of bookmarks in the list.
 * @bookmarks: the list of bookmarks.
 * 
 * Return value: the length of the bookmark list.
 **/
guint
nemo_bookmark_list_length (NemoBookmarkList *bookmarks)
{
	g_return_val_if_fail (NEMO_IS_BOOKMARK_LIST(bookmarks), 0);

	return g_list_length (bookmarks->list);
}

static GList *
load_bookmark_metadata_file (NemoBookmarkList *list)
{   /* Part of load_files thread */
    GError *error = NULL;
    GKeyFile *kfile = g_key_file_new ();
    GList *ret = NULL;

    gchar *filename;

    filename = g_build_filename (g_get_user_config_dir (),
                                 "nemo",
                                 "bookmark-metadata",
                                 NULL);

    if (g_key_file_load_from_file (kfile,
                                   filename,
                                   G_KEY_FILE_NONE,
                                   &error)) {
        gchar **items = g_key_file_get_groups (kfile, NULL);

        guint i;
        for (i = 0; i < g_strv_length (items); i++) {
            NemoBookmarkMetadata *meta = nemo_bookmark_metadata_new ();

            meta->bookmark_name = g_strdup (items[i]);
            meta->icon_uri = g_key_file_get_string (kfile,
                                                    items[i],
                                                    KEY_ICON_URI,
                                                    NULL);
            meta->icon_name = g_key_file_get_string (kfile,
                                                     items[i],
                                                     KEY_ICON_NAME,
                                                     NULL);
            meta->emblems = g_key_file_get_string_list (kfile,
                                                        items[i],
                                                        KEY_ICON_EMBLEMS,
                                                        NULL,
                                                        NULL);

            ret = g_list_append (ret, meta);
        }

        g_strfreev (items);
    } else if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
        g_warning ("Could not load bookmark metadata file: %s\n", error->message);
        g_error_free (error);
    }

    g_key_file_free (kfile);
    g_free (filename);

    return ret;
}

static void
load_files_finish (NemoBookmarkList *bookmarks,
                   GObject          *source,
                   GAsyncResult     *res)
{
    GError *error = NULL;

    GList *new_list = g_task_propagate_pointer (G_TASK (res), &error);

    if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
        g_warning ("Could not load bookmarks: %s\n", error->message);
        g_error_free (error);
        return;
    }

    GList *old_list = bookmarks->list;
    bookmarks->list = new_list;

    clear_bookmarks (bookmarks, old_list);

    g_list_foreach (bookmarks->list, (GFunc) connect_bookmark_signals, bookmarks);
    g_signal_emit (bookmarks, signals[CHANGED], 0);
}

static gint
find_meta_func (gconstpointer a,
                gconstpointer b)
{
    return g_strcmp0 (((NemoBookmarkMetadata *) a)->bookmark_name, (gchar *) b);
}

static void
load_files_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
    NemoBookmarkList *list = NEMO_BOOKMARK_LIST (source_object);

    GError *error = NULL;
    GFile *file;

    file = nemo_bookmark_list_get_file ();

    if (!g_file_query_exists (file, NULL)) {
        g_object_unref (file);
        file = nemo_bookmark_list_get_legacy_file ();
    }

    gchar *contents = NULL;

    GList *new_list = NULL;

    if (g_file_load_contents (file,
                              cancellable,
                              &contents,
                              NULL,
                              NULL,
                              &error)) {

        GList *metadata = load_bookmark_metadata_file (list);

        gchar **lines;
        gint i;

        lines = g_strsplit (contents, "\n", -1);
        for (i = 0; lines[i]; i++) {
            /* Ignore empty or invalid lines that cannot be parsed properly */
            if (lines[i][0] != '\0' && lines[i][0] != ' ') {
                /* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
                /* we must seperate the bookmark uri and the potential label */
                gchar *space, *label;

                label = NULL;
                space = strchr (lines[i], ' ');
                if (space) {
                    *space = '\0';
                    label = g_strdup (space + 1);
                }

                GList *meta_ptr = g_list_find_custom (metadata, label, (GCompareFunc) find_meta_func);

                NemoBookmarkMetadata *md = NULL;

                if (meta_ptr) {
                    metadata = g_list_remove_link (metadata, meta_ptr);
                    md = (NemoBookmarkMetadata *) meta_ptr->data;
                    g_list_free (meta_ptr);
                }

                new_list = g_list_insert (new_list,
                                          new_bookmark_from_uri (lines[i], label, md),
                                          -1);

                g_free (label);
            }
        }

        g_list_free_full (metadata, (GDestroyNotify) nemo_bookmark_metadata_free);

        g_free (contents);
        g_strfreev (lines);

        g_task_return_pointer (task, new_list, NULL);
    } else {
        g_task_return_error (task, error);
    }

    g_object_unref (file);
}

static void
load_files_async (NemoBookmarkList    *self,
                  GAsyncReadyCallback  callback)
{
    GTask *task;

    g_object_ref (self);

    task = g_task_new (self, NULL, callback, self);
    g_task_run_in_thread (task, load_files_thread);

    g_object_unref (task);
}

static void
save_bookmark_metadata_file (NemoBookmarkList *list)
{   /* Part of save_files thread */
    GError *error = NULL;
    GKeyFile *kfile = g_key_file_new ();

    gchar *filename;

    filename = g_build_filename (g_get_user_config_dir (),
                                 "nemo",
                                 "bookmark-metadata",
                                 NULL);

    GFile *file = g_file_new_for_path (filename);
    GFile *parent = g_file_get_parent (file);

    gchar *path = g_file_get_path (parent);
    g_mkdir_with_parents (path, 0700);

    g_free (path);
    g_object_unref (parent);

    GList *ptr;

    for (ptr = list->list; ptr != NULL; ptr = ptr->next) {
        NemoBookmark *bookmark = NEMO_BOOKMARK (ptr->data);

        NemoBookmarkMetadata *data = nemo_bookmark_get_updated_metadata (bookmark);

        if (data == NULL)
            continue;

        if (data->icon_uri)
            g_key_file_set_string (kfile,
                                   nemo_bookmark_get_name (bookmark),
                                   KEY_ICON_URI,
                                   data->icon_uri);

        if (data->icon_name)
            g_key_file_set_string (kfile,
                                   nemo_bookmark_get_name (bookmark),
                                   KEY_ICON_NAME,
                                   data->icon_name);

        if (data->emblems) {
            g_key_file_set_string_list (kfile,
                                        nemo_bookmark_get_name (bookmark),
                                        KEY_ICON_EMBLEMS,
                                        (const gchar * const *) data->emblems,
                                        g_strv_length (data->emblems));
        }

        nemo_bookmark_metadata_free (data);
    }

    if (g_key_file_save_to_file (kfile,
                                 filename,
                                 &error)) {
        ensure_proper_file_permissions (file);
    } else {
        g_warning ("Could not save bookmark metadata file: %s\n", error->message);
        g_error_free (error);
    }

    g_free (filename);
    g_key_file_free (kfile);
    g_object_unref (file);
}

static void
save_files_finish (NemoBookmarkList *bookmarks,
                   GObject          *source,
                   GAsyncResult     *res)
{
    GError *error = NULL;

    g_task_propagate_boolean (G_TASK (res), &error);

    if (error != NULL) {
        g_warning ("Unable to replace contents of the bookmarks file: %s",
        error->message);
        g_error_free (error);
    }

    GFile *file = nemo_bookmark_list_get_file ();

    /* re-enable bookmark file monitoring */
    bookmarks->monitor = g_file_monitor_file (file, 0, NULL, NULL);
    g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);
    g_signal_connect (bookmarks->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb),
                      bookmarks);

    g_object_unref (file);
}

static void
save_files_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
    NemoBookmarkList *bookmarks = NEMO_BOOKMARK_LIST (source_object);

    GFile *file;
    GList *l;
    GString *bookmark_string;
    GFile *parent;
    char *path;

    /* temporarily disable bookmark file monitoring when writing file */
    if (bookmarks->monitor != NULL) {
        g_file_monitor_cancel (bookmarks->monitor);
        bookmarks->monitor = NULL;
    }

    file = nemo_bookmark_list_get_file ();

    bookmark_string = g_string_new (NULL);

    for (l = bookmarks->list; l; l = l->next) {
        NemoBookmark *bookmark;

        bookmark = NEMO_BOOKMARK (l->data);

        const char *label;
        char *uri;
        label = nemo_bookmark_get_name (bookmark);
        uri = nemo_bookmark_get_uri (bookmark);
        g_string_append_printf (bookmark_string,
                    "%s %s\n", uri, label);
        g_free (uri);
    }

    parent = g_file_get_parent (file);
    path = g_file_get_path (parent);
    g_mkdir_with_parents (path, 0700);
    g_free (path);
    g_object_unref (parent);

    GError *error = NULL;

    if (g_file_replace_contents (file,
                                 bookmark_string->str,
                                 bookmark_string->len,
                                 NULL,
                                 FALSE,
                                 0,
                                 NULL,
                                 NULL,
                                 &error)) {
        ensure_proper_file_permissions (file);
        g_task_return_boolean (task, TRUE);
    } else {
        g_task_return_error (task, error);
    }

    g_string_free (bookmark_string, TRUE);

    g_object_unref (file);

    save_bookmark_metadata_file (bookmarks);
}

static void
save_files_async (NemoBookmarkList    *self,
                  GAsyncReadyCallback  callback)
{
    GTask *task;

    g_object_ref (self);
    task = g_task_new (self, NULL, callback, self);
    g_task_run_in_thread (task, save_files_thread);

    g_object_unref (task);
}

static void
process_next_op (NemoBookmarkList *bookmarks);

static void
op_processed_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
	NemoBookmarkList *self = user_data;
	int op;

	op = GPOINTER_TO_INT (g_queue_pop_tail (self->pending_ops));

	if (op == LOAD_JOB) {
		load_files_finish (self, source, res);
	} else {
		save_files_finish (self, source, res);
	}

	if (!g_queue_is_empty (self->pending_ops)) {
		process_next_op (self);
	}

	/* release the reference acquired during the _async method */
	g_object_unref (self);
}

static void
process_next_op (NemoBookmarkList *bookmarks)
{
	gint op;

	op = GPOINTER_TO_INT (g_queue_peek_tail (bookmarks->pending_ops));

	if (op == LOAD_JOB) {
		load_files_async (bookmarks, op_processed_cb);
	} else {
		save_files_async (bookmarks, op_processed_cb);
	}
}

/**
 * nemo_bookmark_list_load_file:
 * 
 * Reads bookmarks from file, clobbering contents in memory.
 * @bookmarks: the list of bookmarks to fill with file contents.
 **/
static void
nemo_bookmark_list_load_file (NemoBookmarkList *bookmarks)
{
	g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (LOAD_JOB));

	if (g_queue_get_length (bookmarks->pending_ops) == 1) {
		process_next_op (bookmarks);
	}
}

/**
 * nemo_bookmark_list_save_file:
 * 
 * Save bookmarks to disk.
 * @bookmarks: the list of bookmarks to save.
 **/
static void
nemo_bookmark_list_save_file (NemoBookmarkList *bookmarks)
{
	g_signal_emit (bookmarks, signals[CHANGED], 0);

	g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (SAVE_JOB));

	if (g_queue_get_length (bookmarks->pending_ops) == 1) {
		process_next_op (bookmarks);
	}
}

static NemoBookmarkList *list = NULL;

/**
 * nemo_bookmark_list_get_default:
 * 
 * Retrieves the bookmark list singleton, with contents read from disk.
 * 
 * Return value: A pointer to the object
 **/
NemoBookmarkList *
nemo_bookmark_list_get_default (void)
{
    if (list == NULL) {
        list = NEMO_BOOKMARK_LIST (g_object_new (NEMO_TYPE_BOOKMARK_LIST, NULL));
    }

    return list;
}

/**
 * nemo_bookmark_list_set_window_geometry:
 * 
 * Set a bookmarks window's geometry (position & size), in string form. This is
 * stored to disk by this class, and can be retrieved later in
 * the same session or in a future session.
 * @bookmarks: the list of bookmarks associated with the window.
 * @geometry: the new window geometry string.
 **/
void
nemo_bookmark_list_set_window_geometry (NemoBookmarkList *bookmarks,
					    const char           *geometry)
{
	g_return_if_fail (NEMO_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (geometry != NULL);

	g_free (window_geometry);
	window_geometry = g_strdup (geometry);

	nemo_bookmark_list_save_file (bookmarks);
}


