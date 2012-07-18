/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NemoUndoManager - Manages undo and redo transactions.
 *                       This is the public interface used by the application.                      
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
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

#ifndef NEMO_UNDO_MANAGER_H
#define NEMO_UNDO_MANAGER_H

#include <libnemo-private/nemo-undo.h>

#define NEMO_TYPE_UNDO_MANAGER nemo_undo_manager_get_type()
#define NEMO_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_UNDO_MANAGER, NemoUndoManager))
#define NEMO_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_UNDO_MANAGER, NemoUndoManagerClass))
#define NEMO_IS_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_UNDO_MANAGER))
#define NEMO_IS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_UNDO_MANAGER))
#define NEMO_UNDO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_UNDO_MANAGER, NemoUndoManagerClass))
	
typedef struct NemoUndoManagerDetails NemoUndoManagerDetails;

typedef struct {
	GObject parent;
	NemoUndoManagerDetails *details;
} NemoUndoManager;

typedef struct {
	GObjectClass parent_slot;
	void (* changed) (GObject *object, gpointer data);
} NemoUndoManagerClass;

GType                nemo_undo_manager_get_type                           (void);
NemoUndoManager *nemo_undo_manager_new                                (void);

/* Undo operations. */
void                 nemo_undo_manager_undo                               (NemoUndoManager *undo_manager);

/* Attach the undo manager to a Gtk object so that object and the widgets inside it can participate in undo. */
void                 nemo_undo_manager_attach                             (NemoUndoManager *manager,
									       GObject             *object);

void		nemo_undo_manager_append (NemoUndoManager *manager,
					      NemoUndoTransaction *transaction);
void            nemo_undo_manager_forget (NemoUndoManager *manager,
					      NemoUndoTransaction *transaction);

#endif /* NEMO_UNDO_MANAGER_H */
