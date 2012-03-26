/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusEntry: one-line text editing widget. This consists of bug fixes
 * and other improvements to GtkEntry, and all the changes could be rolled
 * into GtkEntry some day.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_ENTRY_H
#define NAUTILUS_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_ENTRY nautilus_entry_get_type()
#define NAUTILUS_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_ENTRY, NautilusEntry))
#define NAUTILUS_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ENTRY, NautilusEntryClass))
#define NAUTILUS_IS_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_ENTRY))
#define NAUTILUS_IS_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ENTRY))
#define NAUTILUS_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_ENTRY, NautilusEntryClass))

typedef struct NautilusEntryDetails NautilusEntryDetails;

typedef struct {
	GtkEntry parent;
	NautilusEntryDetails *details;
} NautilusEntry;

typedef struct {
	GtkEntryClass parent_class;

	void (*user_changed)      (NautilusEntry *entry);
	void (*selection_changed) (NautilusEntry *entry);
} NautilusEntryClass;

GType       nautilus_entry_get_type                 (void);
GtkWidget  *nautilus_entry_new                      (void);
GtkWidget  *nautilus_entry_new_with_max_length      (guint16        max);
void        nautilus_entry_set_text                 (NautilusEntry *entry,
						     const char    *text);
void        nautilus_entry_select_all               (NautilusEntry *entry);
void        nautilus_entry_select_all_at_idle       (NautilusEntry *entry);
void        nautilus_entry_set_special_tab_handling (NautilusEntry *entry,
						     gboolean       special_tab_handling);

G_END_DECLS

#endif /* NAUTILUS_ENTRY_H */
