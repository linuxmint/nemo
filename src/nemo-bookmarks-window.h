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

/* nemo-bookmarks-window.h - interface for bookmark-editing window.
 */

#ifndef NEMO_BOOKMARKS_WINDOW_H
#define NEMO_BOOKMARKS_WINDOW_H

#include <gtk/gtk.h>

#include "nemo-window.h"
#include "nemo-bookmark-list.h"

#define NEMO_TYPE_BOOKMARKS_WINDOW nemo_bookmarks_window_get_type()
#define NEMO_BOOKMARKS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_BOOKMARKS_WINDOW, NemoBookmarksWindow))
#define NEMO_BOOKMARKS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_BOOKMARKS_WINDOW, NemoBookmarksWindowClass))
#define NEMO_IS_BOOKMARKS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_BOOKMARKS_WINDOW))
#define NEMO_IS_BOOKMARKS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_BOOKMARKS_WINDOW))
#define NEMO_BOOKMARKS_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_BOOKMARKS_WINDOW, NemoBookmarksWindowClass))

typedef struct NemoBookmarksWindowPrivate NemoBookmarksWindowPrivate;

typedef struct  {
	GtkWindow parent;

	NemoBookmarksWindowPrivate *priv;
} NemoBookmarksWindow;

typedef struct {
	GtkWindowClass parent_class;
} NemoBookmarksWindowClass;

GType nemo_bookmarks_window_get_type (void);

GtkWindow *nemo_bookmarks_window_new (NemoWindow       *parent_window);

#endif /* NEMO_BOOKMARKS_WINDOW_H */
