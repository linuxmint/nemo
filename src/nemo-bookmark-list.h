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

/* nemo-bookmark-list.h - interface for centralized list of bookmarks.
 */

#ifndef NEMO_BOOKMARK_LIST_H
#define NEMO_BOOKMARK_LIST_H

#include <libnemo-private/nemo-bookmark.h>
#include <gio/gio.h>

typedef struct NemoBookmarkList NemoBookmarkList;
typedef struct NemoBookmarkListClass NemoBookmarkListClass;

#define NEMO_TYPE_BOOKMARK_LIST nemo_bookmark_list_get_type()
#define NEMO_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_BOOKMARK_LIST, NemoBookmarkList))
#define NEMO_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_BOOKMARK_LIST, NemoBookmarkListClass))
#define NEMO_IS_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_BOOKMARK_LIST))
#define NEMO_IS_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_BOOKMARK_LIST))
#define NEMO_BOOKMARK_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_BOOKMARK_LIST, NemoBookmarkListClass))

struct NemoBookmarkList {
	GObject object;

	GList *list; 
	GFileMonitor *monitor;
	GQueue *pending_ops;
};

struct NemoBookmarkListClass {
	GObjectClass parent_class;
	void (* changed) (NemoBookmarkList *bookmarks);
};

GType                   nemo_bookmark_list_get_type            (void);
NemoBookmarkList *  nemo_bookmark_list_new                 (void);
void                    nemo_bookmark_list_append              (NemoBookmarkList   *bookmarks,
								    NemoBookmark *bookmark);
gboolean                nemo_bookmark_list_contains            (NemoBookmarkList   *bookmarks,
								    NemoBookmark *bookmark);
void                    nemo_bookmark_list_delete_item_at      (NemoBookmarkList   *bookmarks,
								    guint                   index);
void                    nemo_bookmark_list_delete_items_with_uri (NemoBookmarkList *bookmarks,
								    const char		   *uri);
void                    nemo_bookmark_list_insert_item         (NemoBookmarkList   *bookmarks,
								    NemoBookmark *bookmark,
								    guint                   index);
guint                   nemo_bookmark_list_length              (NemoBookmarkList   *bookmarks);
NemoBookmark *      nemo_bookmark_list_item_at             (NemoBookmarkList   *bookmarks,
								    guint                   index);
void                    nemo_bookmark_list_move_item           (NemoBookmarkList *bookmarks,
								    guint                 index,
								    guint                 destination);
void                    nemo_bookmark_list_sort_ascending           (NemoBookmarkList *bookmarks);
void                    nemo_bookmark_list_set_window_geometry (NemoBookmarkList   *bookmarks,
								    const char             *geometry);
const char *            nemo_bookmark_list_get_window_geometry (NemoBookmarkList   *bookmarks);

#endif /* NEMO_BOOKMARK_LIST_H */
