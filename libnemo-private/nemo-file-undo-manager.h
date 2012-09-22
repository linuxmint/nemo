/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-undo-manager.h - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#ifndef __NEMO_FILE_UNDO_MANAGER_H__
#define __NEMO_FILE_UNDO_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libnemo-private/nemo-file-undo-operations.h>

typedef struct _NemoFileUndoManager NemoFileUndoManager;
typedef struct _NemoFileUndoManagerClass NemoFileUndoManagerClass;
typedef struct _NemoFileUndoManagerPrivate NemoFileUndoManagerPrivate;

#define NEMO_TYPE_FILE_UNDO_MANAGER\
	(nemo_file_undo_manager_get_type())
#define NEMO_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_CAST((object), NEMO_TYPE_FILE_UNDO_MANAGER,\
				    NemoFileUndoManager))
#define NEMO_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST((klass), NEMO_TYPE_FILE_UNDO_MANAGER,\
				 NemoFileUndoManagerClass))
#define NEMO_IS_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_TYPE((object), NEMO_TYPE_FILE_UNDO_MANAGER))
#define NEMO_IS_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE((klass), NEMO_TYPE_FILE_UNDO_MANAGER))
#define NEMO_FILE_UNDO_MANAGER_GET_CLASS(object)\
	(G_TYPE_INSTANCE_GET_CLASS((object), NEMO_TYPE_FILE_UNDO_MANAGER,\
				   NemoFileUndoManagerClass))

typedef enum {
	NEMO_FILE_UNDO_MANAGER_STATE_NONE,
	NEMO_FILE_UNDO_MANAGER_STATE_UNDO,
	NEMO_FILE_UNDO_MANAGER_STATE_REDO
} NemoFileUndoManagerState;

struct _NemoFileUndoManager {
	GObject parent_instance;

	/* < private > */
	NemoFileUndoManagerPrivate* priv;
};

struct _NemoFileUndoManagerClass {
	GObjectClass parent_class;
};

GType nemo_file_undo_manager_get_type (void) G_GNUC_CONST;

NemoFileUndoManager * nemo_file_undo_manager_get (void);

void nemo_file_undo_manager_set_action (NemoFileUndoInfo *info);
NemoFileUndoInfo *nemo_file_undo_manager_get_action (void);

NemoFileUndoManagerState nemo_file_undo_manager_get_state (void);

void nemo_file_undo_manager_undo (GtkWindow *parent_window);
void nemo_file_undo_manager_redo (GtkWindow *parent_window);

void nemo_file_undo_manager_push_flag (void);
gboolean nemo_file_undo_manager_pop_flag (void);

#endif /* __NEMO_FILE_UNDO_MANAGER_H__ */
