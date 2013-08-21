/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-bookmark.h - implementation of individual bookmarks.
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef NEMO_BOOKMARK_H
#define NEMO_BOOKMARK_H

#include <gtk/gtk.h>
#include <gio/gio.h>
typedef struct NemoBookmark NemoBookmark;

#define NEMO_TYPE_BOOKMARK nemo_bookmark_get_type()
#define NEMO_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_BOOKMARK, NemoBookmark))
#define NEMO_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_BOOKMARK, NemoBookmarkClass))
#define NEMO_IS_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_BOOKMARK))
#define NEMO_IS_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_BOOKMARK))
#define NEMO_BOOKMARK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_BOOKMARK, NemoBookmarkClass))

typedef struct NemoBookmarkDetails NemoBookmarkDetails;

struct NemoBookmark {
	GObject object;
	NemoBookmarkDetails *details;	
};

struct NemoBookmarkClass {
	GObjectClass parent_class;

	/* Signals that clients can connect to. */

	/* The contents-changed signal is emitted when the bookmark's contents
	 * (custom name or URI) changed.
	 */
	void	(* contents_changed) (NemoBookmark *bookmark);
};

typedef struct NemoBookmarkClass NemoBookmarkClass;

GType                 nemo_bookmark_get_type               (void);
NemoBookmark *    nemo_bookmark_new                    (GFile *location,
                                                                const char *custom_name,
                                                                GIcon *icon);
NemoBookmark *    nemo_bookmark_copy                   (NemoBookmark      *bookmark);
const char *          nemo_bookmark_get_name               (NemoBookmark      *bookmark);
GFile *               nemo_bookmark_get_location           (NemoBookmark      *bookmark);
char *                nemo_bookmark_get_uri                (NemoBookmark      *bookmark);
GIcon *               nemo_bookmark_get_icon               (NemoBookmark      *bookmark);
gboolean              nemo_bookmark_get_exists             (NemoBookmark      *bookmark);
gboolean	      nemo_bookmark_get_has_custom_name    (NemoBookmark      *bookmark);		
void                  nemo_bookmark_set_custom_name        (NemoBookmark      *bookmark,
								const char            *new_name);		
gboolean              nemo_bookmark_uri_known_not_to_exist (NemoBookmark      *bookmark);
int                   nemo_bookmark_compare_with           (gconstpointer          a,
								gconstpointer          b);
int                   nemo_bookmark_compare_uris           (gconstpointer          a,
								gconstpointer          b);

void                  nemo_bookmark_set_scroll_pos         (NemoBookmark      *bookmark,
								const char            *uri);
char *                nemo_bookmark_get_scroll_pos         (NemoBookmark      *bookmark);


/* Helper functions for displaying bookmarks */
GtkWidget *           nemo_bookmark_menu_item_new          (NemoBookmark      *bookmark);

#endif /* NEMO_BOOKMARK_H */
