/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef NAUTILUS_LOCATION_ENTRY_H
#define NAUTILUS_LOCATION_ENTRY_H

#include <libnautilus-private/nautilus-entry.h>

#define NAUTILUS_TYPE_LOCATION_ENTRY nautilus_location_entry_get_type()
#define NAUTILUS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_LOCATION_ENTRY, NautilusLocationEntry))
#define NAUTILUS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LOCATION_ENTRY, NautilusLocationEntryClass))
#define NAUTILUS_IS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_LOCATION_ENTRY))
#define NAUTILUS_IS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LOCATION_ENTRY))
#define NAUTILUS_LOCATION_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_LOCATION_ENTRY, NautilusLocationEntryClass))

typedef struct NautilusLocationEntryDetails NautilusLocationEntryDetails;

typedef struct NautilusLocationEntry {
	NautilusEntry parent;
	NautilusLocationEntryDetails *details;
} NautilusLocationEntry;

typedef struct {
	NautilusEntryClass parent_class;
} NautilusLocationEntryClass;

typedef enum {
	NAUTILUS_LOCATION_ENTRY_ACTION_GOTO,
	NAUTILUS_LOCATION_ENTRY_ACTION_CLEAR
} NautilusLocationEntryAction;

GType      nautilus_location_entry_get_type     	(void);
GtkWidget* nautilus_location_entry_new          	(void);
void       nautilus_location_entry_set_special_text     (NautilusLocationEntry *entry,
							 const char            *special_text);
void       nautilus_location_entry_set_secondary_action (NautilusLocationEntry *entry,
							 NautilusLocationEntryAction secondary_action);
void       nautilus_location_entry_update_current_location (NautilusLocationEntry *entry,
							    const char *path);

#endif /* NAUTILUS_LOCATION_ENTRY_H */
