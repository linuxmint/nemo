/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-stock-dialogs.c: Various standard dialogs for Eel.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "eel-stock-dialogs.h"

#include "eel-glib-extensions.h"
#include "eel-gtk-extensions.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#define TIMED_WAIT_STANDARD_DURATION 2000
#define TIMED_WAIT_MIN_TIME_UP 3000

#define TIMED_WAIT_MINIMUM_DIALOG_WIDTH 300

#define RESPONSE_DETAILS 1000

typedef struct {
	EelCancelCallback cancel_callback;
	gpointer callback_data;

	/* Parameters for creation of the window. */
	char *wait_message;
	GtkWindow *parent_window;

	/* Timer to determine when we need to create the window. */
	guint timeout_handler_id;
	
	/* Window, once it's created. */
	GtkDialog *dialog;
	
	/* system time (microseconds) when dialog was created */
	gint64 dialog_creation_time;

} TimedWait;

static GHashTable *timed_wait_hash_table;

static void timed_wait_dialog_destroy_callback (GtkWidget *object, gpointer callback_data);

static guint
timed_wait_hash (gconstpointer value)
{
	const TimedWait *wait;

	wait = value;

	return GPOINTER_TO_UINT (wait->cancel_callback)
		^ GPOINTER_TO_UINT (wait->callback_data);
}

static gboolean
timed_wait_hash_equal (gconstpointer value1, gconstpointer value2)
{
	const TimedWait *wait1, *wait2;

	wait1 = value1;
	wait2 = value2;

	return wait1->cancel_callback == wait2->cancel_callback
		&& wait1->callback_data == wait2->callback_data;
}

static void
timed_wait_delayed_close_destroy_dialog_callback (GtkWidget *object, gpointer callback_data)
{
	g_source_remove (GPOINTER_TO_UINT (callback_data));
}

static gboolean
timed_wait_delayed_close_timeout_callback (gpointer callback_data)
{
	guint handler_id;

	handler_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (callback_data),
							  "eel-stock-dialogs/delayed_close_handler_timeout_id"));
	
	g_signal_handlers_disconnect_by_func (G_OBJECT (callback_data),
					      G_CALLBACK (timed_wait_delayed_close_destroy_dialog_callback),
					      GUINT_TO_POINTER (handler_id));
	
	gtk_widget_destroy (GTK_WIDGET (callback_data));

	return FALSE;
}

static void
timed_wait_free (TimedWait *wait)
{
	guint delayed_close_handler_id;
	guint64 time_up;

	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) != NULL);

	g_hash_table_remove (timed_wait_hash_table, wait);

	g_free (wait->wait_message);
	if (wait->parent_window != NULL) {
		g_object_unref (wait->parent_window);
	}
	if (wait->timeout_handler_id != 0) {
		g_source_remove (wait->timeout_handler_id);
	}
	if (wait->dialog != NULL) {
		/* Make sure to detach from the "destroy" signal, or we'll
		 * double-free.
		 */
		g_signal_handlers_disconnect_by_func (G_OBJECT (wait->dialog),
						      G_CALLBACK (timed_wait_dialog_destroy_callback),
						      wait);

		/* compute time up in milliseconds */
		time_up = (eel_get_system_time () - wait->dialog_creation_time) / 1000;
		
		if (time_up < TIMED_WAIT_MIN_TIME_UP) {
			delayed_close_handler_id = g_timeout_add (TIMED_WAIT_MIN_TIME_UP - time_up,
			                                            timed_wait_delayed_close_timeout_callback,
			                                            wait->dialog);
			g_object_set_data (G_OBJECT (wait->dialog),
					     "eel-stock-dialogs/delayed_close_handler_timeout_id",
					     GUINT_TO_POINTER (delayed_close_handler_id));
			g_signal_connect (wait->dialog, "destroy",
					    G_CALLBACK (timed_wait_delayed_close_destroy_dialog_callback),
					    GUINT_TO_POINTER (delayed_close_handler_id));
		} else {
			gtk_widget_destroy (GTK_WIDGET (wait->dialog));
		}
	}

	/* And the wait object itself. */
	g_free (wait);
}

static void
timed_wait_dialog_destroy_callback (GtkWidget *object, gpointer callback_data)
{
	TimedWait *wait;

	wait = callback_data;

	g_assert (GTK_DIALOG (object) == wait->dialog);

	wait->dialog = NULL;
	
	/* When there's no cancel_callback, the originator will/must
	 * call eel_timed_wait_stop which will call timed_wait_free.
	 */

	if (wait->cancel_callback != NULL) {
		(* wait->cancel_callback) (wait->callback_data);
		timed_wait_free (wait);
	}
}

static void
trash_dialog_response_callback (GtkDialog *dialog,
				int response_id,
				TimedWait *wait)
{
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static gboolean
timed_wait_callback (gpointer callback_data)
{
	TimedWait *wait;
	GtkDialog *dialog;
	const char *button;

	wait = callback_data;

	/* Put up the timed wait window. */
	button = wait->cancel_callback != NULL ? GTK_STOCK_CANCEL : GTK_STOCK_OK;
	dialog = GTK_DIALOG (gtk_message_dialog_new (wait->parent_window,
						     0,
						     GTK_MESSAGE_INFO,
						     GTK_BUTTONS_NONE,
						     NULL));

	g_object_set (dialog,
		      "text", wait->wait_message,
		      "secondary-text", _("You can stop this operation by clicking cancel."),
		      NULL);

	gtk_dialog_add_button (GTK_DIALOG (dialog), button, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	/* The contents are often very small, causing tiny little
	 * dialogs with their titles clipped if you just let gtk
	 * sizing do its thing. This enforces a minimum width to
	 * make it more likely that the title won't be clipped.
	 */
	gtk_window_set_default_size (GTK_WINDOW (dialog),
				     TIMED_WAIT_MINIMUM_DIALOG_WIDTH,
				     -1);
	wait->dialog_creation_time = eel_get_system_time ();
	gtk_widget_show (GTK_WIDGET (dialog));

	/* FIXME bugzilla.eazel.com 2441: 
	 * Could parent here, but it's complicated because we
	 * don't want this window to go away just because the parent
	 * would go away first.
	 */

	/* Make the dialog cancel the timed wait when it goes away.
	 * Connect to "destroy" instead of "response" since we want
	 * to be called no matter how the dialog goes away.
	 */
	g_signal_connect (dialog, "destroy",
			  G_CALLBACK (timed_wait_dialog_destroy_callback),
			  wait);
	g_signal_connect (dialog, "response",
			  G_CALLBACK (trash_dialog_response_callback),
			  wait);

	wait->timeout_handler_id = 0;
	wait->dialog = dialog;
	
	return FALSE;
}

void
eel_timed_wait_start_with_duration (int duration,
					 EelCancelCallback cancel_callback,
					 gpointer callback_data,
					 const char *wait_message,
					 GtkWindow *parent_window)
{
	TimedWait *wait;
	
	g_return_if_fail (callback_data != NULL);
	g_return_if_fail (wait_message != NULL);
	g_return_if_fail (parent_window == NULL || GTK_IS_WINDOW (parent_window));

	/* Create the timed wait record. */
	wait = g_new0 (TimedWait, 1);
	wait->wait_message = g_strdup (wait_message);
	wait->cancel_callback = cancel_callback;
	wait->callback_data = callback_data;
	wait->parent_window = parent_window;
	
	if (parent_window != NULL) {
		g_object_ref (parent_window);
	}

	/* Start the timer. */
	wait->timeout_handler_id = g_timeout_add (duration, timed_wait_callback, wait);

	/* Put in the hash table so we can find it later. */
	if (timed_wait_hash_table == NULL) {
		timed_wait_hash_table = g_hash_table_new (timed_wait_hash, timed_wait_hash_equal);
	}
	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == NULL);
	g_hash_table_insert (timed_wait_hash_table, wait, wait);
	g_assert (g_hash_table_lookup (timed_wait_hash_table, wait) == wait);
}

void
eel_timed_wait_start (EelCancelCallback cancel_callback,
			   gpointer callback_data,
			   const char *wait_message,
			   GtkWindow *parent_window)
{
	eel_timed_wait_start_with_duration
		(TIMED_WAIT_STANDARD_DURATION,
		 cancel_callback, callback_data,
		 wait_message, parent_window);
}

void
eel_timed_wait_stop (EelCancelCallback cancel_callback,
			  gpointer callback_data)
{
	TimedWait key;
	TimedWait *wait;

	g_return_if_fail (callback_data != NULL);
	
	key.cancel_callback = cancel_callback;
	key.callback_data = callback_data;
	wait = g_hash_table_lookup (timed_wait_hash_table, &key);

	g_return_if_fail (wait != NULL);

	timed_wait_free (wait);
}

int
eel_run_simple_dialog (GtkWidget *parent, gboolean ignore_close_box,
		       GtkMessageType message_type, const char *primary_text,
		       const char *secondary_text, ...)
{
	va_list button_title_args;
	const char *button_title;
        GtkWidget *dialog;
        GtkWidget *top_widget, *chosen_parent;
	int result;
	int response_id;

	/* Parent it if asked to. */
	chosen_parent = NULL;
        if (parent != NULL) {
		top_widget = gtk_widget_get_toplevel (parent);
		if (GTK_IS_WINDOW (top_widget)) {
			chosen_parent = top_widget;
		}
	}
	
	/* Create the dialog. */
	dialog = gtk_message_dialog_new (GTK_WINDOW (chosen_parent), 
					 0,
					 message_type,
					 GTK_BUTTONS_NONE,
					 NULL);

	g_object_set (dialog,
		      "text", primary_text,
		      "secondary-text", secondary_text,
		      NULL);

	va_start (button_title_args, secondary_text);
	response_id = 0;
	while (1) {
		button_title = va_arg (button_title_args, const char *);
		if (button_title == NULL) {
			break;
		}
		gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, response_id);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);
		response_id++;
	}
	va_end (button_title_args);

	/* Run it. */
        gtk_widget_show (dialog);
        result = gtk_dialog_run (GTK_DIALOG (dialog));
	while ((result == GTK_RESPONSE_NONE || result == GTK_RESPONSE_DELETE_EVENT) && ignore_close_box) {
		gtk_widget_show (GTK_WIDGET (dialog));
		result = gtk_dialog_run (GTK_DIALOG (dialog));
	}
	gtk_widget_destroy (dialog);

	return result;
}

static GtkDialog *
create_message_dialog (const char *primary_text,
		       const char *secondary_text,
		       GtkMessageType type,
		       GtkButtonsType buttons_type,
		       GtkWindow *parent)
{  
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (parent,
					 0,
					 type,
					 buttons_type,
					 NULL);

	g_object_set (dialog,
		      "text", primary_text,
		      "secondary-text", secondary_text,
		      NULL);

	return GTK_DIALOG (dialog);
}

static GtkDialog *
show_message_dialog (const char *primary_text,
		     const char *secondary_text,
		     GtkMessageType type,
		     GtkButtonsType buttons_type,
		     const char *details_text,
		     GtkWindow *parent)
{
	GtkDialog *dialog;

	dialog = create_message_dialog (primary_text, secondary_text, type, 
					buttons_type, parent);
	if (details_text != NULL) {
		eel_gtk_message_dialog_set_details_label (GTK_MESSAGE_DIALOG (dialog),
							  details_text);
	}
	gtk_widget_show (GTK_WIDGET (dialog));

	g_signal_connect (dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	return dialog;
}

static GtkDialog *
show_ok_dialog (const char *primary_text,
		const char *secondary_text,
		GtkMessageType type,
		GtkWindow *parent)
{  
	GtkDialog *dialog;

	dialog = show_message_dialog (primary_text, secondary_text, type,
				      GTK_BUTTONS_OK, NULL, parent);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
	
	return dialog;
}

GtkDialog *
eel_create_info_dialog (const char *primary_text,
			const char *secondary_text,
			GtkWindow *parent)
{
	return create_message_dialog (primary_text, secondary_text,
				      GTK_MESSAGE_INFO,
				      GTK_BUTTONS_OK,
				      parent);
}

GtkDialog *
eel_show_info_dialog (const char *primary_text,
		      const char *secondary_text,
		      GtkWindow *parent)
{
	return show_ok_dialog (primary_text, 
			    secondary_text,
			    GTK_MESSAGE_INFO, parent);
}

GtkDialog *
eel_show_info_dialog_with_details (const char *primary_text,
				   const char *secondary_text,
				   const char *detailed_info,
				   GtkWindow *parent)
{
	GtkDialog *dialog;

	if (detailed_info == NULL
	    || strcmp (primary_text, detailed_info) == 0) {
		return eel_show_info_dialog (primary_text, secondary_text, parent);
	}

	dialog = show_message_dialog (primary_text,
				      secondary_text,
				      GTK_MESSAGE_INFO, 
				      GTK_BUTTONS_OK,
				      detailed_info,
				      parent);

	return dialog;

}


GtkDialog *
eel_show_warning_dialog (const char *primary_text,
			 const char *secondary_text,
			 GtkWindow *parent)
{
	return show_ok_dialog (primary_text, 
			       secondary_text,
			       GTK_MESSAGE_WARNING, parent);
}


GtkDialog *
eel_show_error_dialog (const char *primary_text,
		       const char *secondary_text,
		       GtkWindow *parent)
{
	return show_ok_dialog (primary_text,
			       secondary_text,
			       GTK_MESSAGE_ERROR, parent);
}

GtkDialog *
eel_show_error_dialog_with_details (const char *primary_text,
				    const char *secondary_text,
				    const char *detailed_error_message,
				    GtkWindow *parent)
{
	GtkDialog *dialog;

	g_return_val_if_fail (primary_text != NULL, NULL);
	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

	if (detailed_error_message == NULL
	    || strcmp (primary_text, detailed_error_message) == 0) {
		return eel_show_error_dialog (primary_text, secondary_text, parent);
	}
	
	dialog = show_message_dialog (primary_text, 
				      secondary_text,
				      GTK_MESSAGE_ERROR,
				      GTK_BUTTONS_OK, detailed_error_message,
				      parent);
	return dialog;
}

/**
 * eel_show_yes_no_dialog:
 * 
 * Create and show a dialog asking a question with two choices.
 * The caller needs to set up any necessary callbacks 
 * for the buttons. Use eel_create_question_dialog instead
 * if any visual changes need to be made, to avoid flashiness.
 * @question: The text of the question.
 * @yes_label: The label of the "yes" button.
 * @no_label: The label of the "no" button.
 * @parent: The parent window for this dialog.
 */
GtkDialog *
eel_show_yes_no_dialog (const char *primary_text, 
			const char *secondary_text,
			const char *yes_label,
			const char *no_label,
			GtkWindow *parent)
{
	GtkDialog *dialog = NULL;
	dialog = eel_create_question_dialog (primary_text,
					     secondary_text,
					     no_label, GTK_RESPONSE_CANCEL,
					     yes_label, GTK_RESPONSE_YES,
					     GTK_WINDOW (parent));
	gtk_widget_show (GTK_WIDGET (dialog));
	return dialog;
}

/**
 * eel_create_question_dialog:
 * 
 * Create a dialog asking a question with at least two choices.
 * The caller needs to set up any necessary callbacks 
 * for the buttons. The dialog is not yet shown, so that the
 * caller can add additional buttons or make other visual changes
 * without causing flashiness.
 * @question: The text of the question.
 * @answer_0: The label of the leftmost button (index 0)
 * @answer_1: The label of the 2nd-to-leftmost button (index 1)
 * @parent: The parent window for this dialog.
 */
GtkDialog *
eel_create_question_dialog (const char *primary_text,
			    const char *secondary_text,
			    const char *answer_1,
			    int response_1,
			    const char *answer_2,
			    int response_2,
			    GtkWindow *parent)
{
	GtkDialog *dialog;
	
	dialog = create_message_dialog (primary_text,
					secondary_text,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_NONE,
					parent);
	gtk_dialog_add_buttons (dialog, answer_1, response_1, answer_2, response_2, NULL);
	return dialog;
}
