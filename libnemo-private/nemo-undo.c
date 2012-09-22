/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-undo.c - public interface for objects that implement
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

#include <config.h>
#include "nemo-undo.h"

#include "nemo-undo-private.h"
#include "nemo-undo-transaction.h"
#include <gtk/gtk.h>

#define NEMO_UNDO_MANAGER_DATA "Nemo undo manager"

/* Register a simple undo action by calling nemo_undo_register_full. */
void
nemo_undo_register (GObject *target,
			NemoUndoCallback callback,
			gpointer callback_data,
			GDestroyNotify callback_data_destroy_notify,
			const char *operation_name,
			const char *undo_menu_item_label,
			const char *undo_menu_item_hint,
			const char *redo_menu_item_label,
			const char *redo_menu_item_hint)
{
	NemoUndoAtom atom;
	GList single_atom_list;

	g_return_if_fail (G_IS_OBJECT (target));
	g_return_if_fail (callback != NULL);

	/* Make an atom. */
	atom.target = target;
	atom.callback = callback;
	atom.callback_data = callback_data;
	atom.callback_data_destroy_notify = callback_data_destroy_notify;

	/* Make a single-atom list. */
	single_atom_list.data = &atom;
	single_atom_list.next = NULL;
	single_atom_list.prev = NULL;

	/* Call the full version of the registration function,
	 * using the undo target as the place to search for the
	 * undo manager.
	 */
	nemo_undo_register_full (&single_atom_list,
				     target,
				     operation_name,
				     undo_menu_item_label,
				     undo_menu_item_hint,
				     redo_menu_item_label,
				     redo_menu_item_hint);
}

/* Register an undo action. */
void
nemo_undo_register_full (GList *atoms,
			     GObject *undo_manager_search_start_object,
			     const char *operation_name,
			     const char *undo_menu_item_label,
			     const char *undo_menu_item_hint,
			     const char *redo_menu_item_label,
			     const char *redo_menu_item_hint)
{
	NemoUndoTransaction *transaction;
	GList *p;

	g_return_if_fail (atoms != NULL);
	g_return_if_fail (G_IS_OBJECT (undo_manager_search_start_object));

	/* Create an undo transaction */
	transaction = nemo_undo_transaction_new (operation_name,
						     undo_menu_item_label,
						     undo_menu_item_hint, 
						     redo_menu_item_label,
						     redo_menu_item_hint);
	for (p = atoms; p != NULL; p = p->next) {
		nemo_undo_transaction_add_atom (transaction, p->data);
	}
	nemo_undo_transaction_add_to_undo_manager
		(transaction,
		 nemo_undo_get_undo_manager (undo_manager_search_start_object));

	/* Now we are done with the transaction.
	 * If the undo manager is holding it, then this will not destroy it.
	 */
	g_object_unref (transaction);
}

/* Cover for forgetting about all undo relating to a particular target. */
void
nemo_undo_unregister (GObject *target)
{
	/* Perhaps this should also unregister all children if called on a
	 * GtkContainer? That might be handy.
	 */
	nemo_undo_transaction_unregister_object (target);
}

void
nemo_undo (GObject *undo_manager_search_start_object)
{
	NemoUndoManager *manager;

	g_return_if_fail (G_IS_OBJECT (undo_manager_search_start_object));

	manager = nemo_undo_get_undo_manager (undo_manager_search_start_object);
	if (manager != NULL) {
		nemo_undo_manager_undo (manager);
	}
}

NemoUndoManager *
nemo_undo_get_undo_manager (GObject *start_object)
{
	NemoUndoManager *manager;
	GtkWidget *parent;
	GtkWindow *transient_parent;

	if (start_object == NULL) {
		return NULL;
	}

	g_return_val_if_fail (G_IS_OBJECT (start_object), NULL);

	/* Check for an undo manager right here. */
	manager = g_object_get_data (start_object, NEMO_UNDO_MANAGER_DATA);
	if (manager != NULL) {
		return manager;
	}

	/* Check for undo manager up the parent chain. */
	if (GTK_IS_WIDGET (start_object)) {
		parent = gtk_widget_get_parent (GTK_WIDGET (start_object));
		if (parent != NULL) {
			manager = nemo_undo_get_undo_manager (G_OBJECT (parent));
			if (manager != NULL) {
				return manager;
			}
		}

		/* Check for undo manager in our window's parent. */
		if (GTK_IS_WINDOW (start_object)) {
			transient_parent = gtk_window_get_transient_for (GTK_WINDOW (start_object));
			if (transient_parent != NULL) {
				manager = nemo_undo_get_undo_manager (G_OBJECT (transient_parent));
				if (manager != NULL) {
					return manager;
				}
			}
		}
	}
	
	/* Found nothing. I can live with that. */
	return NULL;
}

void
nemo_undo_attach_undo_manager (GObject *object,
				   NemoUndoManager *manager)
{
	g_return_if_fail (G_IS_OBJECT (object));

	if (manager == NULL) {
		g_object_set_data (object, NEMO_UNDO_MANAGER_DATA, NULL);
	} else {
		g_object_ref (manager);
		g_object_set_data_full
			(object, NEMO_UNDO_MANAGER_DATA,
			 manager, g_object_unref);
	}
}

/* Copy a reference to the undo manager fromone object to another. */
void
nemo_undo_share_undo_manager (GObject *destination_object,
				  GObject *source_object)
{
	NemoUndoManager *manager;

	manager = nemo_undo_get_undo_manager (source_object);
	nemo_undo_attach_undo_manager (destination_object, manager);
}
