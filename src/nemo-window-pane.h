/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-window-pane.h: Nemo window pane

   Copyright (C) 2008 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Holger Berndt <berndth@gmx.de>
*/

#ifndef NEMO_WINDOW_PANE_H
#define NEMO_WINDOW_PANE_H

#include <glib-object.h>

#include "nemo-window.h"

#include <libnemo-private/nemo-icon-info.h>

#define NEMO_TYPE_WINDOW_PANE	 (nemo_window_pane_get_type())
#define NEMO_WINDOW_PANE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_WINDOW_PANE, NemoWindowPaneClass))
#define NEMO_WINDOW_PANE(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_WINDOW_PANE, NemoWindowPane))
#define NEMO_IS_WINDOW_PANE(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_WINDOW_PANE))
#define NEMO_IS_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_WINDOW_PANE))
#define NEMO_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_WINDOW_PANE, NemoWindowPaneClass))

struct _NemoWindowPaneClass {
	GtkBoxClass parent_class;
};

/* A NemoWindowPane is a layer between a slot and a window.
 * Each slot is contained in one pane, and each pane can contain
 * one or more slots. It also supports the notion of an "active slot".
 * On the other hand, each pane is contained in a window, while each
 * window can contain one or multiple panes. Likewise, the window has
 * the notion of an "active pane".
 *
 * A navigation window may have one or more panes.
 */
struct _NemoWindowPane {
	GtkBox parent;

	/* hosting window */
	NemoWindow *window;

	/* available slots, and active slot.
	 * Both of them may never be NULL. */
	GList *slots;
	NemoWindowSlot *active_slot;

	/* location bar */
	GtkWidget *location_bar;
	GtkWidget *path_bar;
	GtkWidget *search_bar;
	GtkWidget *tool_bar;

	gboolean temporary_navigation_bar;
	gboolean temporary_search_bar;

	/* notebook */
	GtkWidget *notebook;

	GtkActionGroup *action_group;

	GtkWidget *last_focus_widget;
};

GType nemo_window_pane_get_type (void);

NemoWindowPane *nemo_window_pane_new (NemoWindow *window);

NemoWindowSlot *nemo_window_pane_open_slot  (NemoWindowPane *pane,
						     NemoWindowOpenSlotFlags flags);
void                nemo_window_pane_close_slot (NemoWindowPane *pane,
						     NemoWindowSlot *slot);

void nemo_window_pane_sync_location_widgets (NemoWindowPane *pane);
void nemo_window_pane_sync_search_widgets  (NemoWindowPane *pane);
void nemo_window_pane_set_active (NemoWindowPane *pane, gboolean is_active);
void nemo_window_pane_slot_close (NemoWindowPane *pane, NemoWindowSlot *slot);

void nemo_window_pane_grab_focus (NemoWindowPane *pane);

/* bars */
void     nemo_window_pane_ensure_location_bar (NemoWindowPane *pane);

#endif /* NEMO_WINDOW_PANE_H */
