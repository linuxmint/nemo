/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-undo.h - public interface for objects that implement
 *                   undoable actions -- works across components
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#ifndef NEMO_UNDO_H
#define NEMO_UNDO_H

#include <glib-object.h>

typedef struct _NemoUndoTransaction NemoUndoTransaction;


/* The basic undoable operation. */
typedef void (* NemoUndoCallback) (GObject *target, gpointer callback_data);

/* Recipe for undo of a bit of work on an object.
 * Create these atoms when you want to register more
 * than one as a single undoable operation.
 */
typedef struct {
	GObject *target;
	NemoUndoCallback callback;
	gpointer callback_data;
	GDestroyNotify callback_data_destroy_notify;
} NemoUndoAtom;

/* Registering something that can be undone. */
void nemo_undo_register              (GObject              *target,
					  NemoUndoCallback  callback,
					  gpointer              callback_data,
					  GDestroyNotify        callback_data_destroy_notify,
					  const char           *operation_name,
					  const char           *undo_menu_item_label,
					  const char           *undo_menu_item_hint,
					  const char           *redo_menu_item_label,
					  const char           *redo_menu_item_hint);
void nemo_undo_register_full         (GList                *atoms,
					  GObject              *undo_manager_search_start_object,
					  const char           *operation_name,
					  const char           *undo_menu_item_label,
					  const char           *undo_menu_item_hint,
					  const char           *redo_menu_item_label,
					  const char           *redo_menu_item_hint);
void nemo_undo_unregister            (GObject              *target);

/* Performing an undo explicitly. Only for use by objects "out in the field".
 * The menu bar itself uses a richer API in the undo manager.
 */
void nemo_undo                       (GObject              *undo_manager_search_start_object);

/* Connecting an undo manager. */
void nemo_undo_share_undo_manager    (GObject              *destination_object,
					  GObject              *source_object);

#endif /* NEMO_UNDO_H */
