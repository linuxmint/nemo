/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NemoUndoTransaction - An object for an undoable transaction.
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

#ifndef NEMO_UNDO_TRANSACTION_H
#define NEMO_UNDO_TRANSACTION_H

#include <libnemo-private/nemo-undo.h>

#define NEMO_TYPE_UNDO_TRANSACTION nemo_undo_transaction_get_type()
#define NEMO_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_UNDO_TRANSACTION, NemoUndoTransaction))
#define NEMO_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_UNDO_TRANSACTION, NemoUndoTransactionClass))
#define NEMO_IS_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_UNDO_TRANSACTION))
#define NEMO_IS_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_UNDO_TRANSACTION))
#define NEMO_UNDO_TRANSACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_UNDO_TRANSACTION, NemoUndoTransactionClass))

/* The typedef for NemoUndoTransaction is in nemo-undo.h
   to avoid circular deps */
typedef struct _NemoUndoTransactionClass NemoUndoTransactionClass;

struct _NemoUndoTransaction {
	GObject parent_slot;
	
	char *operation_name;
	char *undo_menu_item_label;
	char *undo_menu_item_hint;
	char *redo_menu_item_label;
	char *redo_menu_item_hint;
	GList *atom_list;

	NemoUndoManager *owner;
};

struct _NemoUndoTransactionClass {
	GObjectClass parent_slot;
};

GType                    nemo_undo_transaction_get_type            (void);
NemoUndoTransaction *nemo_undo_transaction_new                 (const char              *operation_name,
									const char              *undo_menu_item_label,
									const char              *undo_menu_item_hint,
									const char              *redo_menu_item_label,
									const char              *redo_menu_item_hint);
void                     nemo_undo_transaction_add_atom            (NemoUndoTransaction *transaction,
									const NemoUndoAtom  *atom);
void                     nemo_undo_transaction_add_to_undo_manager (NemoUndoTransaction *transaction,
									NemoUndoManager     *manager);
void                     nemo_undo_transaction_unregister_object   (GObject                 *atom_target);
void                     nemo_undo_transaction_undo                (NemoUndoTransaction *transaction);

#endif /* NEMO_UNDO_TRANSACTION_H */
