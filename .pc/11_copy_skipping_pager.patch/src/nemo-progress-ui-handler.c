/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nemo-progress-ui-handler.c: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nemo-progress-ui-handler.h"

#include "nemo-application.h"
#include "nemo-progress-info-widget.h"

#include <glib/gi18n.h>

#include <libnemo-private/nemo-progress-info.h>
#include <libnemo-private/nemo-progress-info-manager.h>

#include <libnotify/notify.h>

struct _NemoProgressUIHandlerPriv {
	NemoProgressInfoManager *manager;

	GtkWidget *progress_window;
	GtkWidget *window_vbox;
	guint active_infos;

	NotifyNotification *progress_notification;
	GtkStatusIcon *status_icon;
};

G_DEFINE_TYPE (NemoProgressUIHandler, nemo_progress_ui_handler, G_TYPE_OBJECT);

/* Our policy for showing progress notification is the following:
 * - file operations that end within two seconds do not get notified in any way
 * - if no file operations are running, and one passes the two seconds
 *   timeout, a window is displayed with the progress
 * - if the window is closed, we show a resident notification, or a status icon, depending on
 *   the capabilities of the notification daemon running in the session
 * - if some file operations are running, and another one passes the two seconds
 *   timeout, and the window is showing, we add it to the window directly
 * - in the same case, but when the window is not showing, we update the resident
 *   notification, changing its message, or the status icon's tooltip
 * - when one file operation finishes, if it's not the last one, we only update the
 *   resident notification's message, or the status icon's tooltip
 * - in the same case, if it's the last one, we close the resident notification,
 *   or the status icon, and trigger a transient one
 * - in the same case, but the window was showing, we just hide the window
 */

#define ACTION_DETAILS "details"

static gboolean server_has_persistence (void);

static void
status_icon_activate_cb (GtkStatusIcon *icon,
			 NemoProgressUIHandler *self)
{	
	gtk_status_icon_set_visible (icon, FALSE);
	gtk_window_present (GTK_WINDOW (self->priv->progress_window));
}

static void
notification_show_details_cb (NotifyNotification *notification,
			      char *action_name,
			      gpointer user_data)
{
	NemoProgressUIHandler *self = user_data;


	if (g_strcmp0 (action_name, ACTION_DETAILS) != 0) {
		return;
	}

	notify_notification_close (self->priv->progress_notification, NULL);
	gtk_window_present (GTK_WINDOW (self->priv->progress_window));
}

static void
progress_ui_handler_ensure_notification (NemoProgressUIHandler *self)
{
	NotifyNotification *notify;

	if (self->priv->progress_notification) {
		return;
	}

	notify = notify_notification_new (_("File Operations"),
					  NULL, NULL);
	self->priv->progress_notification = notify;

	notify_notification_set_category (notify, "transfer");
	notify_notification_set_hint (notify, "resident",
				      g_variant_new_boolean (TRUE));

	notify_notification_add_action (notify, ACTION_DETAILS,
					_("Show Details"),
					notification_show_details_cb,
					self,
					NULL);
}

static void
progress_ui_handler_ensure_status_icon (NemoProgressUIHandler *self)
{
	GIcon *icon;
	GtkStatusIcon *status_icon;

	if (self->priv->status_icon != NULL) {
		return;
	}

	icon = g_themed_icon_new_with_default_fallbacks ("system-file-manager-symbolic");
	status_icon = gtk_status_icon_new_from_gicon (icon);
	g_signal_connect (status_icon, "activate",
			  (GCallback) status_icon_activate_cb,
			  self);

	gtk_status_icon_set_visible (status_icon, FALSE);
	g_object_unref (icon);

	self->priv->status_icon = status_icon;
}

static void
progress_ui_handler_update_notification (NemoProgressUIHandler *self)
{
	gchar *body;

	progress_ui_handler_ensure_notification (self);

	body = g_strdup_printf (ngettext ("%'d file operation active",
					  "%'d file operations active",
					  self->priv->active_infos),
				self->priv->active_infos);

	notify_notification_update (self->priv->progress_notification,
				    _("File Operations"),
				    body,
				    NULL);

	notify_notification_show (self->priv->progress_notification, NULL);

	g_free (body);
}

static void
progress_ui_handler_update_status_icon (NemoProgressUIHandler *self)
{
	gchar *tooltip;

	progress_ui_handler_ensure_status_icon (self);

	tooltip = g_strdup_printf (ngettext ("%'d file operation active",
					     "%'d file operations active",
					     self->priv->active_infos),
				   self->priv->active_infos);
	gtk_status_icon_set_tooltip_text (self->priv->status_icon, tooltip);
	g_free (tooltip);

	gtk_status_icon_set_visible (self->priv->status_icon, TRUE);
}

static gboolean
progress_window_delete_event (GtkWidget *widget,
			      GdkEvent *event,
			      NemoProgressUIHandler *self)
{
	gtk_widget_hide (widget);

	if (server_has_persistence ()) {
		progress_ui_handler_update_notification (self);
	} else {
		progress_ui_handler_update_status_icon (self);
	}

	return TRUE;
}

static void
progress_ui_handler_ensure_window (NemoProgressUIHandler *self)
{
	GtkWidget *vbox, *progress_window;
	
	if (self->priv->progress_window != NULL) {
		return;
	}
	
	progress_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	self->priv->progress_window = progress_window;
	gtk_window_set_resizable (GTK_WINDOW (progress_window),
				  FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (progress_window), 10);
 
	gtk_window_set_title (GTK_WINDOW (progress_window),
			      _("File Operations"));
	gtk_window_set_wmclass (GTK_WINDOW (progress_window),
				"file_progress", "Nemo");
	gtk_window_set_position (GTK_WINDOW (progress_window),
				 GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name (GTK_WINDOW (progress_window),
				"system-file-manager");

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_set_spacing (GTK_BOX (vbox), 5);
	gtk_container_add (GTK_CONTAINER (progress_window),
			   vbox);
	self->priv->window_vbox = vbox;
	gtk_widget_show (vbox);

	g_signal_connect (progress_window,
			  "delete-event",
			  (GCallback) progress_window_delete_event, self);
}

static void
progress_ui_handler_update_notification_or_status (NemoProgressUIHandler *self)
{
	if (server_has_persistence ()) {
		progress_ui_handler_update_notification (self);
	} else {
		progress_ui_handler_update_status_icon (self);
	}
}

static void
progress_ui_handler_add_to_window (NemoProgressUIHandler *self,
				   NemoProgressInfo *info)
{
	GtkWidget *progress;

	progress = nemo_progress_info_widget_new (info);
	progress_ui_handler_ensure_window (self);

	gtk_box_pack_start (GTK_BOX (self->priv->window_vbox),
			    progress,
			    FALSE, FALSE, 6);

	gtk_widget_show (progress);
}

static void
progress_ui_handler_show_complete_notification (NemoProgressUIHandler *self)
{
	NotifyNotification *complete_notification;

	/* don't display the notification if we'd be using a status icon */
	if (!server_has_persistence ()) {
		return;
	}

	complete_notification = notify_notification_new (_("File Operations"),
							 _("All file operations have been successfully completed"),
							 NULL);
	notify_notification_show (complete_notification, NULL);

	g_object_unref (complete_notification);
}

static void
progress_ui_handler_hide_notification_or_status (NemoProgressUIHandler *self)
{
	if (self->priv->status_icon != NULL) {
		gtk_status_icon_set_visible (self->priv->status_icon, FALSE);
	}

	if (self->priv->progress_notification != NULL) {
		notify_notification_close (self->priv->progress_notification, NULL);
		g_clear_object (&self->priv->progress_notification);
	}
}

static void
progress_info_finished_cb (NemoProgressInfo *info,
			   NemoProgressUIHandler *self)
{
	self->priv->active_infos--;

	if (self->priv->active_infos > 0) {
		if (!gtk_widget_get_visible (self->priv->progress_window)) {
			progress_ui_handler_update_notification_or_status (self);
		}
	} else {
		if (gtk_widget_get_visible (self->priv->progress_window)) {
			gtk_widget_hide (self->priv->progress_window);
		} else {
			progress_ui_handler_hide_notification_or_status (self);
			progress_ui_handler_show_complete_notification (self);
		}
	}
}

static void
handle_new_progress_info (NemoProgressUIHandler *self,
			  NemoProgressInfo *info)
{
	g_signal_connect (info, "finished",
			  G_CALLBACK (progress_info_finished_cb), self);

	self->priv->active_infos++;

	if (self->priv->active_infos == 1) {
		/* this is the only active operation, present the window */
		progress_ui_handler_add_to_window (self, info);
		gtk_window_present (GTK_WINDOW (self->priv->progress_window));
	} else {
		if (gtk_widget_get_visible (self->priv->progress_window)) {
			progress_ui_handler_add_to_window (self, info);
		} else {
			progress_ui_handler_update_notification_or_status (self);
		}
	}
}

typedef struct {
	NemoProgressInfo *info;
	NemoProgressUIHandler *self;
} TimeoutData;

static void
timeout_data_free (TimeoutData *data)
{
	g_clear_object (&data->self);
	g_clear_object (&data->info);

	g_slice_free (TimeoutData, data);
}

static TimeoutData *
timeout_data_new (NemoProgressUIHandler *self,
		  NemoProgressInfo *info)
{
	TimeoutData *retval;

	retval = g_slice_new0 (TimeoutData);
	retval->self = g_object_ref (self);
	retval->info = g_object_ref (info);

	return retval;
}

static gboolean
new_op_started_timeout (TimeoutData *data)
{
	NemoProgressInfo *info = data->info;
	NemoProgressUIHandler *self = data->self;

	if (nemo_progress_info_get_is_paused (info)) {
		return TRUE;
	}

	if (!nemo_progress_info_get_is_finished (info)) {
		handle_new_progress_info (self, info);
	}

	timeout_data_free (data);

	return FALSE;
}

static void
release_application (NemoProgressInfo *info,
		     NemoProgressUIHandler *self)
{
	NemoApplication *app;

	/* release the GApplication hold we acquired */
	app = nemo_application_get_singleton ();
	g_application_release (G_APPLICATION (app));
}

static void
progress_info_started_cb (NemoProgressInfo *info,
			  NemoProgressUIHandler *self)
{
	NemoApplication *app;
	TimeoutData *data;

	/* hold GApplication so we never quit while there's an operation pending */
	app = nemo_application_get_singleton ();
	g_application_hold (G_APPLICATION (app));

	g_signal_connect (info, "finished",
			  G_CALLBACK (release_application), self);

	data = timeout_data_new (self, info);

	/* timeout for the progress window to appear */
	g_timeout_add_seconds (2,
			       (GSourceFunc) new_op_started_timeout,
			       data);
}

static void
new_progress_info_cb (NemoProgressInfoManager *manager,
		      NemoProgressInfo *info,
		      NemoProgressUIHandler *self)
{
	g_signal_connect (info, "started",
			  G_CALLBACK (progress_info_started_cb), self);
}

static void
nemo_progress_ui_handler_dispose (GObject *obj)
{
	NemoProgressUIHandler *self = NEMO_PROGRESS_UI_HANDLER (obj);

	g_clear_object (&self->priv->manager);

	G_OBJECT_CLASS (nemo_progress_ui_handler_parent_class)->dispose (obj);
}

static gboolean
server_has_persistence (void)
{
        static gboolean retval = FALSE;
        GList *caps, *l;
        static gboolean initialized = FALSE;

        if (initialized) {
                return retval;
        }
        initialized = TRUE;

        caps = notify_get_server_caps ();
        if (caps == NULL) {
                return FALSE;
        }

        l = g_list_find_custom (caps, "persistence", (GCompareFunc) g_strcmp0);
        retval = (l != NULL);

	g_list_free_full (caps, g_free);

        return retval;
}

static void
nemo_progress_ui_handler_init (NemoProgressUIHandler *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_PROGRESS_UI_HANDLER,
						  NemoProgressUIHandlerPriv);

	self->priv->manager = nemo_progress_info_manager_new ();
	g_signal_connect (self->priv->manager, "new-progress-info",
			  G_CALLBACK (new_progress_info_cb), self);
}

static void
nemo_progress_ui_handler_class_init (NemoProgressUIHandlerClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->dispose = nemo_progress_ui_handler_dispose;
	
	g_type_class_add_private (klass, sizeof (NemoProgressUIHandlerPriv));
}

NemoProgressUIHandler *
nemo_progress_ui_handler_new (void)
{
	return g_object_new (NEMO_TYPE_PROGRESS_UI_HANDLER, NULL);
}
