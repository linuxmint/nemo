/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-undo-manager.c - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010, 2012 Red Hat, Inc.
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
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nemo-file-undo-manager.h"

#include "nemo-file-operations.h"
#include "nemo-file.h"
#include "nemo-trash-monitor.h"

#include <glib/gi18n.h>

#define DEBUG_FLAG NEMO_DEBUG_UNDO
#include "nemo-debug.h"

enum {
	SIGNAL_UNDO_CHANGED,
	NUM_SIGNALS,
};

static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NemoFileUndoManager, nemo_file_undo_manager, G_TYPE_OBJECT)

struct _NemoFileUndoManagerPrivate
{
	NemoFileUndoInfo *info;
	NemoFileUndoManagerState state;
	NemoFileUndoManagerState last_state;

	guint undo_redo_flag : 1;

	gulong trash_signal_id;
};

static NemoFileUndoManager *undo_singleton = NULL;

static NemoFileUndoManager *
get_singleton (void)
{
	if (undo_singleton == NULL) {
		undo_singleton = g_object_new (NEMO_TYPE_FILE_UNDO_MANAGER, NULL);
		g_object_add_weak_pointer (G_OBJECT (undo_singleton), (gpointer) &undo_singleton);
	}

	return undo_singleton;
}

static void
file_undo_manager_clear (NemoFileUndoManager *self)
{
	g_clear_object (&self->priv->info);
	self->priv->state = NEMO_FILE_UNDO_MANAGER_STATE_NONE;
}

static void
trash_state_changed_cb (NemoTrashMonitor *monitor,
			gboolean is_empty,
			gpointer user_data)
{
	NemoFileUndoManager *self = user_data;

	if (!is_empty) {
		return;
	}

	if (self->priv->state == NEMO_FILE_UNDO_MANAGER_STATE_NONE) {
		return;
	}

	if (NEMO_IS_FILE_UNDO_INFO_TRASH (self->priv->info)) {
		file_undo_manager_clear (self);
		g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
	}
}

static void
nemo_file_undo_manager_init (NemoFileUndoManager * self)
{
	NemoFileUndoManagerPrivate *priv = self->priv = 
		G_TYPE_INSTANCE_GET_PRIVATE (self, 
					     NEMO_TYPE_FILE_UNDO_MANAGER, 
					     NemoFileUndoManagerPrivate);

	priv->trash_signal_id = g_signal_connect (nemo_trash_monitor_get (),
						  "trash-state-changed",
						  G_CALLBACK (trash_state_changed_cb), self);
}

static void
nemo_file_undo_manager_finalize (GObject * object)
{
	NemoFileUndoManager *self = NEMO_FILE_UNDO_MANAGER (object);
	NemoFileUndoManagerPrivate *priv = self->priv;

	if (priv->trash_signal_id != 0) {
		g_signal_handler_disconnect (nemo_trash_monitor_get (),
					     priv->trash_signal_id);
		priv->trash_signal_id = 0;
	}

	file_undo_manager_clear (self);

	G_OBJECT_CLASS (nemo_file_undo_manager_parent_class)->finalize (object);
}

static void
nemo_file_undo_manager_class_init (NemoFileUndoManagerClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);

	oclass->finalize = nemo_file_undo_manager_finalize;

	signals[SIGNAL_UNDO_CHANGED] =
		g_signal_new ("undo-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (NemoFileUndoManagerPrivate));
}

static void
undo_info_apply_ready (GObject *source,
		       GAsyncResult *res,
		       gpointer user_data)
{
	NemoFileUndoManager *self = user_data;
	NemoFileUndoInfo *info = NEMO_FILE_UNDO_INFO (source);
	gboolean success, user_cancel;

	success = nemo_file_undo_info_apply_finish (info, res, &user_cancel, NULL);

	/* just return in case we got another another operation set */
	if ((self->priv->info != NULL) &&
	    (self->priv->info != info)) {
		return;
	}

	if (success) {
		if (self->priv->last_state == NEMO_FILE_UNDO_MANAGER_STATE_UNDO) {
			self->priv->state = NEMO_FILE_UNDO_MANAGER_STATE_REDO;
		} else if (self->priv->last_state == NEMO_FILE_UNDO_MANAGER_STATE_REDO) {
			self->priv->state = NEMO_FILE_UNDO_MANAGER_STATE_UNDO;
		}

		self->priv->info = g_object_ref (info);
	} else if (user_cancel) {
		self->priv->state = self->priv->last_state;
		self->priv->info = g_object_ref (info);
	} else {
		file_undo_manager_clear (self);
	}

	g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

static void
do_undo_redo (NemoFileUndoManager *self,
	      GtkWindow *parent_window)
{
	gboolean undo = self->priv->state == NEMO_FILE_UNDO_MANAGER_STATE_UNDO;

	self->priv->last_state = self->priv->state;
	
	nemo_file_undo_manager_push_flag ();
	nemo_file_undo_info_apply_async (self->priv->info, undo, parent_window,
					     undo_info_apply_ready, self);

	/* clear actions while undoing */
	file_undo_manager_clear (self);
	g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

void
nemo_file_undo_manager_redo (GtkWindow *parent_window)
{
	NemoFileUndoManager *self = get_singleton ();

	if (self->priv->state != NEMO_FILE_UNDO_MANAGER_STATE_REDO) {
		g_warning ("Called redo, but state is %s!", self->priv->state == 0 ?
			   "none" : "undo");
		return;
	}

	do_undo_redo (self, parent_window);
}

void
nemo_file_undo_manager_undo (GtkWindow *parent_window)
{
	NemoFileUndoManager *self = get_singleton ();

	if (self->priv->state != NEMO_FILE_UNDO_MANAGER_STATE_UNDO) {
		g_warning ("Called undo, but state is %s!", self->priv->state == 0 ?
			   "none" : "redo");
		return;
	}

	do_undo_redo (self, parent_window);
}

void
nemo_file_undo_manager_set_action (NemoFileUndoInfo *info)
{
	NemoFileUndoManager *self = get_singleton ();

	DEBUG ("Setting undo information %p", info);

	file_undo_manager_clear (self);

	if (info != NULL) {
		self->priv->info = g_object_ref (info);
		self->priv->state = NEMO_FILE_UNDO_MANAGER_STATE_UNDO;
		self->priv->last_state = NEMO_FILE_UNDO_MANAGER_STATE_NONE;
	}

	g_signal_emit (self, signals[SIGNAL_UNDO_CHANGED], 0);
}

NemoFileUndoInfo *
nemo_file_undo_manager_get_action (void)
{
	NemoFileUndoManager *self = get_singleton ();

	return self->priv->info;
}

NemoFileUndoManagerState 
nemo_file_undo_manager_get_state (void)
{
	NemoFileUndoManager *self = get_singleton ();

	return self->priv->state;
}

void
nemo_file_undo_manager_push_flag ()
{
	NemoFileUndoManager *self = get_singleton ();
	NemoFileUndoManagerPrivate *priv = self->priv;

	priv->undo_redo_flag = TRUE;
}

gboolean
nemo_file_undo_manager_pop_flag ()
{
	NemoFileUndoManager *self = get_singleton ();
	NemoFileUndoManagerPrivate *priv = self->priv;
	gboolean retval = FALSE;

	if (priv->undo_redo_flag) {
		retval = TRUE;
	}

	priv->undo_redo_flag = FALSE;
	return retval;
}

NemoFileUndoManager *
nemo_file_undo_manager_get ()
{
	return get_singleton ();
}
