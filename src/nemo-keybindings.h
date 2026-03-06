/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-keybindings.h - Configurable keyboard shortcuts for Nemo.
 *
 * Copyright (C) 2026 Nemo contributors
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
 */

#ifndef NEMO_KEYBINDINGS_H
#define NEMO_KEYBINDINGS_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct {
	const gchar *settings_key;     /* GSettings key name */
	const gchar *accel_path;       /* GtkAccelMap path (NULL for binding-set entries) */
	const gchar *description;      /* Human-readable description */
	const gchar *category;         /* Category for grouping in UI */
	const gchar *default_accel;    /* Default accelerator string */
	const gchar *binding_set_name; /* GtkBindingSet class name (NULL for accel-map entries) */
	const gchar *signal_name;      /* Signal to emit (NULL for accel-map entries) */
} NemoKeybindingEntry;

extern const NemoKeybindingEntry nemo_keybinding_entries[];
extern const gint nemo_keybinding_entries_count;

extern GSettings *nemo_keybinding_settings;

void     nemo_keybindings_init                (void);
void     nemo_keybindings_apply_all           (void);
void     nemo_keybindings_set_for_action      (const gchar *settings_key,
                                               const gchar *accel_string);
GtkWidget *nemo_keybindings_create_editor     (void);

#endif /* NEMO_KEYBINDINGS_H */
