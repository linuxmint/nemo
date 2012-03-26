/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoTransaction - An object for an undoable transaction.
 *                           Used internally by undo machinery.
 *                           Not public.
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

#ifndef NAUTILUS_UNDO_TRANSACTION_H
#define NAUTILUS_UNDO_TRANSACTION_H

#include <libnautilus-private/nautilus-undo.h>

#define NAUTILUS_TYPE_UNDO_TRANSACTION nautilus_undo_transaction_get_type()
#define NAUTILUS_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_UNDO_TRANSACTION, NautilusUndoTransaction))
#define NAUTILUS_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDO_TRANSACTION, NautilusUndoTransactionClass))
#define NAUTILUS_IS_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_UNDO_TRANSACTION))
#define NAUTILUS_IS_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_UNDO_TRANSACTION))
#define NAUTILUS_UNDO_TRANSACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_UNDO_TRANSACTION, NautilusUndoTransactionClass))

/* The typedef for NautilusUndoTransaction is in nautilus-undo.h
   to avoid circular deps */
typedef struct _NautilusUndoTransactionClass NautilusUndoTransactionClass;

struct _NautilusUndoTransaction {
	GObject parent_slot;
	
	char *operation_name;
	char *undo_menu_item_label;
	char *undo_menu_item_hint;
	char *redo_menu_item_label;
	char *redo_menu_item_hint;
	GList *atom_list;

	NautilusUndoManager *owner;
};

struct _NautilusUndoTransactionClass {
	GObjectClass parent_slot;
};

GType                    nautilus_undo_transaction_get_type            (void);
NautilusUndoTransaction *nautilus_undo_transaction_new                 (const char              *operation_name,
									const char              *undo_menu_item_label,
									const char              *undo_menu_item_hint,
									const char              *redo_menu_item_label,
									const char              *redo_menu_item_hint);
void                     nautilus_undo_transaction_add_atom            (NautilusUndoTransaction *transaction,
									const NautilusUndoAtom  *atom);
void                     nautilus_undo_transaction_add_to_undo_manager (NautilusUndoTransaction *transaction,
									NautilusUndoManager     *manager);
void                     nautilus_undo_transaction_unregister_object   (GObject                 *atom_target);
void                     nautilus_undo_transaction_undo                (NautilusUndoTransaction *transaction);

#endif /* NAUTILUS_UNDO_TRANSACTION_H */
