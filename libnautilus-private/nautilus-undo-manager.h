/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoManager - Manages undo and redo transactions.
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

#ifndef NAUTILUS_UNDO_MANAGER_H
#define NAUTILUS_UNDO_MANAGER_H

#include <libnautilus-private/nautilus-undo.h>

#define NAUTILUS_TYPE_UNDO_MANAGER nautilus_undo_manager_get_type()
#define NAUTILUS_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_UNDO_MANAGER, NautilusUndoManager))
#define NAUTILUS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDO_MANAGER, NautilusUndoManagerClass))
#define NAUTILUS_IS_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_UNDO_MANAGER))
#define NAUTILUS_IS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_UNDO_MANAGER))
#define NAUTILUS_UNDO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_UNDO_MANAGER, NautilusUndoManagerClass))
	
typedef struct NautilusUndoManagerDetails NautilusUndoManagerDetails;

typedef struct {
	GObject parent;
	NautilusUndoManagerDetails *details;
} NautilusUndoManager;

typedef struct {
	GObjectClass parent_slot;
	void (* changed) (GObject *object, gpointer data);
} NautilusUndoManagerClass;

GType                nautilus_undo_manager_get_type                           (void);
NautilusUndoManager *nautilus_undo_manager_new                                (void);

/* Undo operations. */
void                 nautilus_undo_manager_undo                               (NautilusUndoManager *undo_manager);

/* Attach the undo manager to a Gtk object so that object and the widgets inside it can participate in undo. */
void                 nautilus_undo_manager_attach                             (NautilusUndoManager *manager,
									       GObject             *object);

void		nautilus_undo_manager_append (NautilusUndoManager *manager,
					      NautilusUndoTransaction *transaction);
void            nautilus_undo_manager_forget (NautilusUndoManager *manager,
					      NautilusUndoTransaction *transaction);

#endif /* NAUTILUS_UNDO_MANAGER_H */
