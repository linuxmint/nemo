/*
 * nemo-window-slot-dnd.c - Handle DnD for widgets acting as
 * NemoWindowSlot proxies
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 * 	    Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef __NEMO_WINDOW_SLOT_DND_H__
#define __NEMO_WINDOW_SLOT_DND_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-dnd.h>

#include "nemo-window-slot.h"

void nemo_drag_slot_proxy_init (GtkWidget *widget,
                                    NemoFile *target_file,
                                    NemoWindowSlot *target_slot);

#endif /* __NEMO_WINDOW_SLOT_DND_H__ */
