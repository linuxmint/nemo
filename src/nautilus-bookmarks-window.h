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
#include "nautilus-bookmark-list.h"

GtkWindow *create_bookmarks_window                 (NautilusBookmarkList *bookmarks,
						    GObject              *undo_manager_source);
void       nautilus_bookmarks_window_save_geometry (GtkWindow            *window);
void	   edit_bookmarks_dialog_set_signals	   (GObject 		 *undo_manager_source);

#endif /* NAUTILUS_BOOKMARKS_WINDOW_H */
