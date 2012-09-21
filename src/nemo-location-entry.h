/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef NEMO_LOCATION_ENTRY_H
#define NEMO_LOCATION_ENTRY_H

#include <libnemo-private/nemo-entry.h>

#define NEMO_TYPE_LOCATION_ENTRY nemo_location_entry_get_type()
#define NEMO_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_LOCATION_ENTRY, NemoLocationEntry))
#define NEMO_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_LOCATION_ENTRY, NemoLocationEntryClass))
#define NEMO_IS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_LOCATION_ENTRY))
#define NEMO_IS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_LOCATION_ENTRY))
#define NEMO_LOCATION_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_LOCATION_ENTRY, NemoLocationEntryClass))

typedef struct NemoLocationEntryDetails NemoLocationEntryDetails;

typedef struct NemoLocationEntry {
	NemoEntry parent;
	NemoLocationEntryDetails *details;
} NemoLocationEntry;

typedef struct {
	NemoEntryClass parent_class;
} NemoLocationEntryClass;

typedef enum {
	NEMO_LOCATION_ENTRY_ACTION_GOTO,
	NEMO_LOCATION_ENTRY_ACTION_CLEAR
} NemoLocationEntryAction;

GType      nemo_location_entry_get_type     	(void);
GtkWidget* nemo_location_entry_new          	(void);
void       nemo_location_entry_set_special_text     (NemoLocationEntry *entry,
							 const char            *special_text);
void       nemo_location_entry_set_secondary_action (NemoLocationEntry *entry,
							 NemoLocationEntryAction secondary_action);
void       nemo_location_entry_update_current_location (NemoLocationEntry *entry,
							    const char *path);

#endif /* NEMO_LOCATION_ENTRY_H */
