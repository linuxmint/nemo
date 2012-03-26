/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author:  Alexander Larsson <alexl@redhat.com>
 */

#ifndef NAUTILUS_WINDOW_BOOKMARKS_H
#define NAUTILUS_WINDOW_BOOKMARKS_H

#include <libnautilus-private/nautilus-bookmark.h>
#include <nautilus-window.h>
#include "nautilus-bookmark-list.h"

void                  nautilus_bookmarks_exiting                        (void);
void                  nautilus_window_add_bookmark_for_current_location (NautilusWindow *window);
void                  nautilus_window_edit_bookmarks                    (NautilusWindow *window);
void                  nautilus_window_initialize_bookmarks_menu         (NautilusWindow *window);

#endif
