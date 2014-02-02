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

#include <gio/gio.h>
#include <string.h>

#define MAX_BOOKMARK_LENGTH 80
#define LOAD_JOB 1
#define SAVE_JOB 2

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* forward declarations */

static void        nemo_bookmark_list_load_file     (NemoBookmarkList *bookmarks);
static void        nemo_bookmark_list_save_file     (NemoBookmarkList *bookmarks);

G_DEFINE_TYPE(NemoBookmarkList, nemo_bookmark_list, G_TYPE_OBJECT)

static NemoBookmark *
new_bookmark_from_uri (const char *uri, const char *label)
{
	NemoBookmark *new_bookmark;
	GFile *location;

	location = NULL;
	if (uri) {
		location = g_file_new_for_uri (uri);
	}
	
	new_bookmark = NULL;

	if (location) {
		new_bookmark = nemo_bookmark_new (location, label, NULL);
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

    if (!g_file_query_exists (file, NULL)) {
        g_object_unref (file);
        g_free(filename);
        return nemo_bookmark_list_get_legacy_file ();
    }

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

static void
bookmark_in_list_notify (GObject *object,
			 GParamSpec *pspec,
			 NemoBookmarkList *bookmarks)
{
	/* emit the changed signal without saving, as only appearance properties changed */
	g_signal_emit (bookmarks, signals[CHANGED], 0);
}

static void
stop_monitoring_bookmark (NemoBookmarkList *bookmarks,
			  NemoBookmark     *bookmark)
{
	g_signal_handlers_disconnect_by_func (bookmark,
					      bookmark_in_list_changed_callback,
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
clear (NemoBookmarkList *bookmarks)
{
	g_list_foreach (bookmarks->list, stop_monitoring_one, bookmarks);
	g_list_free_full (bookmarks->list, g_object_unref);
	bookmarks->list = NULL;
}

static void
do_finalize (GObject *object)
{
	if (NEMO_BOOKMARK_LIST (object)->monitor != NULL) {
		g_file_monitor_cancel (NEMO_BOOKMARK_LIST (object)->monitor);
		NEMO_BOOKMARK_LIST (object)->monitor = NULL;
	}

	g_queue_free (NEMO_BOOKMARK_LIST (object)->pending_ops);

	clear (NEMO_BOOKMARK_LIST (object));

	G_OBJECT_CLASS (nemo_bookmark_list_parent_class)->finalize (object);
}

static void
nemo_bookmark_list_class_init (NemoBookmarkListClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = do_finalize;

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
nemo_bookmark_list_init (NemoBookmarkList *bookmarks)
{
	GFile *file;

	bookmarks->pending_ops = g_queue_new ();

	nemo_bookmark_list_load_file (bookmarks);

	file = nemo_bookmark_list_get_file ();
	bookmarks->monitor = g_file_monitor_file (file, 0, NULL, NULL);
	g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);

	g_signal_connect (bookmarks->monitor, "changed",
			  G_CALLBACK (bookmark_monitor_changed_cb), bookmarks);

	g_object_unref (file);
}

static void
insert_bookmark_internal (NemoBookmarkList *bookmarks,
			  NemoBookmark     *bookmark,
			  int                   index)
{
	bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

	g_signal_connect_object (bookmark, "contents-changed",
				 G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
	g_signal_connect_object (bookmark, "notify::icon",
				 G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);
	g_signal_connect_object (bookmark, "notify::name",
				 G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);
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
	bookmarks->list = g_list_remove_link (bookmarks->list,
					      bookmark_item);

	bookmarks->list = g_list_insert (bookmarks->list,
					 bookmark_item->data,
					 destination);

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

static void
process_next_op (NemoBookmarkList *bookmarks);

static void
op_processed_cb (NemoBookmarkList *self)
{
	g_queue_pop_tail (self->pending_ops);

	if (!g_queue_is_empty (self->pending_ops)) {
		process_next_op (self);
	}
}

static void
load_callback (GObject *source,
	       GAsyncResult *res,
	       gpointer user_data)
{
	NemoBookmarkList *self = NEMO_BOOKMARK_LIST (source);
	gchar *contents;
	char **lines;
	int i;

	contents = g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (res));

	if (contents == NULL) {
		return;
	}

	lines = g_strsplit (contents, "\n", -1);
	for (i = 0; lines[i]; i++) {
		/* Ignore empty or invalid lines that cannot be parsed properly */
		if (lines[i][0] != '\0' && lines[i][0] != ' ') {
			/* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
			/* we must seperate the bookmark uri and the potential label */
			char *space, *label;

			label = NULL;
			space = strchr (lines[i], ' ');
			if (space) {
				*space = '\0';
				label = g_strdup (space + 1);
			}

			insert_bookmark_internal (self, new_bookmark_from_uri (lines[i], label), -1);
			g_free (label);
		}
	}

	g_signal_emit (self, signals[CHANGED], 0);
	op_processed_cb (self);

	g_strfreev (lines);
}

static void
load_io_thread (GSimpleAsyncResult *result,
		GObject *object,
		GCancellable *cancellable)
{
	GFile *file;
	gchar *contents;
	GError *error = NULL;

	file = nemo_bookmark_list_get_file ();

	g_file_load_contents (file, NULL, &contents, NULL, NULL, &error);

	if (error != NULL) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
			g_warning ("Could not load bookmark file: %s\n", error->message);
		}
		g_error_free (error);
	} else {
		g_simple_async_result_set_op_res_gpointer (result, contents, g_free);
	}
}

static void
load_file_async (NemoBookmarkList *self)
{
	GSimpleAsyncResult *result;

	/* Wipe out old list. */
	clear (self);

	result = g_simple_async_result_new (G_OBJECT (self), 
					    load_callback, NULL, NULL);
	g_simple_async_result_run_in_thread (result, load_io_thread,
					     G_PRIORITY_DEFAULT, NULL);
	g_object_unref (result);
}

static void
save_callback (GObject *source,
	       GAsyncResult *res,
	       gpointer user_data)
{
	NemoBookmarkList *self = NEMO_BOOKMARK_LIST (source);
	GFile *file;

	/* re-enable bookmark file monitoring */
	file = nemo_bookmark_list_get_file ();
	self->monitor = g_file_monitor_file (file, 0, NULL, NULL);
	g_object_unref (file);

	g_file_monitor_set_rate_limit (self->monitor, 1000);
	g_signal_connect (self->monitor, "changed",
			  G_CALLBACK (bookmark_monitor_changed_cb), self);

	op_processed_cb (self);
}

static void
save_io_thread (GSimpleAsyncResult *result,
		GObject *object,
		GCancellable *cancellable)
{
	gchar *contents, *path;
	GFile *parent, *file;
	GError *error = NULL;

	file = nemo_bookmark_list_get_file ();
	parent = g_file_get_parent (file);
	path = g_file_get_path (parent);
	g_mkdir_with_parents (path, 0700);
	g_free (path);
	g_object_unref (parent);

	contents = g_simple_async_result_get_op_res_gpointer (result);
	g_file_replace_contents (file, 
				 contents, strlen (contents),
				 NULL, FALSE, 0, NULL,
				 NULL, &error);

	if (error != NULL) {
		g_warning ("Unable to replace contents of the bookmarks file: %s",
			   error->message);
		g_error_free (error);
	}

	g_object_unref (file);
}

static void
save_file_async (NemoBookmarkList *self)
{
	GSimpleAsyncResult *result;
	GString *bookmark_string;
	gchar *contents;
	GList *l;

	bookmark_string = g_string_new (NULL);

	/* temporarily disable bookmark file monitoring when writing file */
	if (self->monitor != NULL) {
		g_file_monitor_cancel (self->monitor);
		self->monitor = NULL;
	}

	for (l = self->list; l; l = l->next) {
		NemoBookmark *bookmark;

		bookmark = NEMO_BOOKMARK (l->data);

		/* make sure we save label if it has one for compatibility with GTK 2.7 and 2.8 */
		if (nemo_bookmark_get_has_custom_name (bookmark)) {
			const char *label;
			char *uri;
			label = nemo_bookmark_get_name (bookmark);
			uri = nemo_bookmark_get_uri (bookmark);
			g_string_append_printf (bookmark_string,
						"%s %s\n", uri, label);
			g_free (uri);
		} else {
			char *uri;
			uri = nemo_bookmark_get_uri (bookmark);
			g_string_append_printf (bookmark_string, "%s\n", uri);
			g_free (uri);
		}
	}

	result = g_simple_async_result_new (G_OBJECT (self),
					    save_callback, NULL, NULL);
	contents = g_string_free (bookmark_string, FALSE);
	g_simple_async_result_set_op_res_gpointer (result, contents, g_free);

	g_simple_async_result_run_in_thread (result, save_io_thread,
					     G_PRIORITY_DEFAULT, NULL);
	g_object_unref (result);
}

static void
process_next_op (NemoBookmarkList *bookmarks)
{
	gint op;

	op = GPOINTER_TO_INT (g_queue_peek_tail (bookmarks->pending_ops));

	if (op == LOAD_JOB) {
		load_file_async (bookmarks);
	} else {
		save_file_async (bookmarks);
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

/**
 * nemo_bookmark_list_new:
 * 
 * Create a new bookmark_list, with contents read from disk.
 * 
 * Return value: A pointer to the new widget.
 **/
NemoBookmarkList *
nemo_bookmark_list_new (void)
{
	NemoBookmarkList *list;

	list = NEMO_BOOKMARK_LIST (g_object_new (NEMO_TYPE_BOOKMARK_LIST, NULL));

	return list;
}
