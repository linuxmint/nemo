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

/* nautilus-bookmarks-window.h - interface for bookmark-editing window.
 */

#ifndef NAUTILUS_BOOKMARKS_WINDOW_H
#define NAUTILUS_BOOKMARKS_WINDOW_H

#include <gtk/gtk.h>

#include "nautilus-window.h"
#include "nautilus-bookmark-list.h"

#define NAUTILUS_TYPE_BOOKMARKS_WINDOW nautilus_bookmarks_window_get_type()
#define NAUTILUS_BOOKMARKS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_BOOKMARKS_WINDOW, NautilusBookmarksWindow))
#define NAUTILUS_BOOKMARKS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BOOKMARKS_WINDOW, NautilusBookmarksWindowClass))
#define NAUTILUS_IS_BOOKMARKS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_BOOKMARKS_WINDOW))
#define NAUTILUS_IS_BOOKMARKS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BOOKMARKS_WINDOW))
#define NAUTILUS_BOOKMARKS_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_BOOKMARKS_WINDOW, NautilusBookmarksWindowClass))

typedef struct NautilusBookmarksWindowPrivate NautilusBookmarksWindowPrivate;

typedef struct  {
	GtkWindow parent;

	NautilusBookmarksWindowPrivate *priv;
} NautilusBookmarksWindow;

typedef struct {
	GtkWindowClass parent_class;
} NautilusBookmarksWindowClass;

GType nautilus_bookmarks_window_get_type (void);

GtkWindow *nautilus_bookmarks_window_new (NautilusWindow       *parent_window);

#endif /* NAUTILUS_BOOKMARKS_WINDOW_H */
