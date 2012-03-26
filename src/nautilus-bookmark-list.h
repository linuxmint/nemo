/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-list.h - interface for centralized list of bookmarks.
 */

#ifndef NAUTILUS_BOOKMARK_LIST_H
#define NAUTILUS_BOOKMARK_LIST_H

#include <libnautilus-private/nautilus-bookmark.h>
#include <gio/gio.h>

typedef struct NautilusBookmarkList NautilusBookmarkList;
typedef struct NautilusBookmarkListClass NautilusBookmarkListClass;

#define NAUTILUS_TYPE_BOOKMARK_LIST nautilus_bookmark_list_get_type()
#define NAUTILUS_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_BOOKMARK_LIST, NautilusBookmarkList))
#define NAUTILUS_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BOOKMARK_LIST, NautilusBookmarkListClass))
#define NAUTILUS_IS_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_BOOKMARK_LIST))
#define NAUTILUS_IS_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BOOKMARK_LIST))
#define NAUTILUS_BOOKMARK_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_BOOKMARK_LIST, NautilusBookmarkListClass))

struct NautilusBookmarkList {
	GObject object;

	GList *list; 
	GFileMonitor *monitor;
	GQueue *pending_ops;
};

struct NautilusBookmarkListClass {
	GObjectClass parent_class;
	void (* changed) (NautilusBookmarkList *bookmarks);
};

GType                   nautilus_bookmark_list_get_type            (void);
NautilusBookmarkList *  nautilus_bookmark_list_new                 (void);
void                    nautilus_bookmark_list_append              (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark);
gboolean                nautilus_bookmark_list_contains            (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark);
void                    nautilus_bookmark_list_delete_item_at      (NautilusBookmarkList   *bookmarks,
								    guint                   index);
void                    nautilus_bookmark_list_delete_items_with_uri (NautilusBookmarkList *bookmarks,
								    const char		   *uri);
void                    nautilus_bookmark_list_insert_item         (NautilusBookmarkList   *bookmarks,
								    NautilusBookmark *bookmark,
								    guint                   index);
guint                   nautilus_bookmark_list_length              (NautilusBookmarkList   *bookmarks);
NautilusBookmark *      nautilus_bookmark_list_item_at             (NautilusBookmarkList   *bookmarks,
								    guint                   index);
void                    nautilus_bookmark_list_move_item           (NautilusBookmarkList *bookmarks,
								    guint                 index,
								    guint                 destination);
void                    nautilus_bookmark_list_set_window_geometry (NautilusBookmarkList   *bookmarks,
								    const char             *geometry);
const char *            nautilus_bookmark_list_get_window_geometry (NautilusBookmarkList   *bookmarks);

#endif /* NAUTILUS_BOOKMARK_LIST_H */
