/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NemoEntry: one-line text editing widget. This consists of bug fixes
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
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NEMO_ENTRY_H
#define NEMO_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_ENTRY nemo_entry_get_type()
#define NEMO_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ENTRY, NemoEntry))
#define NEMO_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ENTRY, NemoEntryClass))
#define NEMO_IS_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ENTRY))
#define NEMO_IS_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ENTRY))
#define NEMO_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ENTRY, NemoEntryClass))

typedef struct NemoEntryDetails NemoEntryDetails;

typedef struct {
	GtkEntry parent;
	NemoEntryDetails *details;
} NemoEntry;

typedef struct {
	GtkEntryClass parent_class;

	void (*selection_changed) (NemoEntry *entry);
} NemoEntryClass;

GType       nemo_entry_get_type                 (void);
GtkWidget  *nemo_entry_new                      (void);
GtkWidget  *nemo_entry_new_with_max_length      (guint16        max);
void        nemo_entry_set_text                 (NemoEntry *entry,
						     const char    *text);
void        nemo_entry_select_all               (NemoEntry *entry);
void        nemo_entry_select_all_at_idle       (NemoEntry *entry);
void        nemo_entry_set_special_tab_handling (NemoEntry *entry,
						     gboolean       special_tab_handling);

G_END_DECLS

#endif /* NEMO_ENTRY_H */
