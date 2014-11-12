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

#include <eel/eel-string.h>

#include <libnemo-private/nemo-progress-info.h>
#include <libnemo-private/nemo-progress-info-manager.h>

#include <libnotify/notify.h>

#ifdef HAVE_UNITY
#include <unity.h>
#include "unity-quicklist-handler.h"
#endif

struct _NemoProgressUIHandlerPriv {
	NemoProgressInfoManager *manager;

	GtkWidget *progress_window;
	GtkWidget *window_vbox;
	guint active_infos;
    guint active_percent;
	GList *infos;

	GtkStatusIcon *status_icon;
    gboolean should_show_status_icon;

#ifdef HAVE_UNITY
	UnityQuicklistHandler *unity_quicklist_handler;
#endif
};

G_DEFINE_TYPE (NemoProgressUIHandler, nemo_progress_ui_handler, G_TYPE_OBJECT);

#define ACTION_DETAILS "details"

static gboolean
status_icon_button_release_cb (GtkStatusIcon *icon,
                                    GdkEvent *event,
			           NemoProgressUIHandler *self)
{	
    self->priv->should_show_status_icon = FALSE;
	gtk_status_icon_set_visible (icon, FALSE);
	gtk_window_present (GTK_WINDOW (self->priv->progress_window));
    return FALSE;
}

static void
progress_ui_handler_ensure_status_icon (NemoProgressUIHandler *self)
{
	GtkStatusIcon *status_icon;

	if (self->priv->status_icon != NULL) {
		return;
	}

	status_icon = gtk_status_icon_new_from_icon_name ("progress-0-symbolic");
	g_signal_connect (status_icon, "button-release-event",
			  (GCallback) status_icon_button_release_cb,
			  self);

	gtk_status_icon_set_visible (status_icon, FALSE);

	self->priv->status_icon = status_icon;
}

static gchar *
get_icon_name_from_percent (guint pct)
{
    gchar *icon_name;
    guint rounded = 0;
    gint ones = pct % 10;

    if (ones < 5)
        rounded = pct - ones;
    else
        rounded = pct + (10 - ones);

    icon_name = g_strdup_printf ("progress-%d-symbolic", rounded);

    return icon_name;
}


static void
progress_ui_handler_update_status_icon (NemoProgressUIHandler *self)
{
	gchar *tooltip;

	progress_ui_handler_ensure_status_icon (self);
    gchar *launchpad_sucks = THOU_TO_STR (self->priv->active_infos);
    tooltip = g_strdup_printf (ngettext ("%1$s file operation active.  %2$d%% complete.",
                               "%1$s file operations active.  %2$d%% complete.",
                               self->priv->active_infos),
                               launchpad_sucks, self->priv->active_percent);
	gtk_status_icon_set_tooltip_text (self->priv->status_icon, tooltip);
    gchar *name = get_icon_name_from_percent (self->priv->active_percent);
    gtk_status_icon_set_from_icon_name (self->priv->status_icon, name);
    g_free (name);
	g_free (tooltip);

	gtk_status_icon_set_visible (self->priv->status_icon, self->priv->should_show_status_icon);
}

#ifdef HAVE_UNITY

static void
progress_ui_handler_unity_progress_changed (NemoProgressInfo *info, NemoProgressUIHandler *self)
{
	g_return_if_fail (self);
	g_return_if_fail (self->priv->unity_quicklist_handler);
	g_return_if_fail (self->priv->manager);

	GList *infos, *l;
	double progress = 0;
	double c, current = 0;
	double t, total = 0;

	infos = nemo_progress_info_manager_get_all_infos (self->priv->manager);

	for (l = infos; l; l = l->next) {
		NemoProgressInfo *i = l->data;
		c = nemo_progress_info_get_current (i);
		t = nemo_progress_info_get_total (i);

		if (c < 0) c = 0;
		if (t <= 0) continue;

		total += t;
		current += c;
	}

	if (current >= 0 && total > 0)
		progress = current / total;

	if (progress > 1.0)
		progress = 1.0;

	for (l = unity_quicklist_get_launcher_entries (self->priv->unity_quicklist_handler); l; l = l->next) {
		UnityLauncherEntry *entry = l->data;
		unity_launcher_entry_set_progress (entry, progress);
	}
}

static gboolean
progress_ui_handler_disable_unity_urgency (UnityLauncherEntry *entry)
{
	g_return_if_fail (entry);

	unity_launcher_entry_set_urgent (entry, FALSE);
	return FALSE;
}

static void
progress_ui_handler_unity_quicklist_show_activated (DbusmenuMenuitem *menu,
                                                    guint timestamp,
                                                    NemoProgressUIHandler *self)
{
	g_return_if_fail (self);

	gtk_window_present_with_time (GTK_WINDOW (self->priv->progress_window), timestamp);
}

static void
progress_ui_handler_unity_quicklist_cancel_activated (DbusmenuMenuitem *menu,
                                                      guint timestamp,
                                                      NemoProgressUIHandler *self)
{
	g_return_if_fail (self);
	g_return_if_fail (self->priv->manager);

	GList *infos, *l;
	infos = nemo_progress_info_manager_get_all_infos (self->priv->manager);

	for (l = infos; l; l = l->next) {
		NemoProgressInfo *info = l->data;
		nemo_progress_info_cancel (info);
	}
}

static DbusmenuMenuitem *
progress_ui_handler_build_unity_quicklist (NemoProgressUIHandler *self)
{
	g_return_if_fail (self);
	GList *l;

	for (l = unity_quicklist_get_launcher_entries (self->priv->unity_quicklist_handler); l; l = l->next) {
		UnityLauncherEntry *entry = l->data;
		DbusmenuMenuitem *ql = unity_launcher_entry_get_quicklist (entry);
		DbusmenuMenuitem *quickmenu;

		if (ql) {
			quickmenu = dbusmenu_menuitem_new ();
			dbusmenu_menuitem_property_set (quickmenu,
			                                DBUSMENU_MENUITEM_PROP_TYPE,
			                                DBUSMENU_CLIENT_TYPES_SEPARATOR);
			dbusmenu_menuitem_property_set_bool (quickmenu,
			                                     DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
			dbusmenu_menuitem_property_set_bool (quickmenu,
			                                     UNITY_QUICKLIST_PROGRESS_ITEM, TRUE);
			unity_quicklist_handler_append_menuitem (entry, quickmenu);
		}

		quickmenu = dbusmenu_menuitem_new ();
		dbusmenu_menuitem_property_set (quickmenu,
		                                DBUSMENU_MENUITEM_PROP_LABEL,
		                                UNITY_QUICKLIST_SHOW_COPY_DIALOG);
		dbusmenu_menuitem_property_set_bool (quickmenu,
		                                     DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_property_set_bool (quickmenu,
		                                     UNITY_QUICKLIST_PROGRESS_ITEM, TRUE);
		unity_quicklist_handler_append_menuitem (entry, quickmenu);
		g_signal_connect (quickmenu, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		                  (GCallback) progress_ui_handler_unity_quicklist_show_activated,
		                  self);

		quickmenu = dbusmenu_menuitem_new ();
		dbusmenu_menuitem_property_set (quickmenu,
		                                DBUSMENU_MENUITEM_PROP_LABEL,
		                                UNITY_QUICKLIST_CANCEL_COPY);
		dbusmenu_menuitem_property_set_bool (quickmenu,
		                                     DBUSMENU_MENUITEM_PROP_VISIBLE, FALSE);
		dbusmenu_menuitem_property_set_bool (quickmenu,
		                                     UNITY_QUICKLIST_PROGRESS_ITEM, TRUE);
		unity_quicklist_handler_append_menuitem (entry, quickmenu);
		g_signal_connect (quickmenu, DBUSMENU_MENUITEM_SIGNAL_ITEM_ACTIVATED,
		                  (GCallback) progress_ui_handler_unity_quicklist_cancel_activated,
		                  self);
	}
}

static void
progress_ui_handler_show_unity_quicklist (NemoProgressUIHandler *self,
                                          UnityLauncherEntry *entry,
                                          gboolean show)
{
	g_return_if_fail (self);
	g_return_if_fail (entry);

	DbusmenuMenuitem *ql;
	GList *children, *l;

	ql = unity_launcher_entry_get_quicklist (entry);
	children = dbusmenu_menuitem_get_children (ql);

	for (l = children; l; l = l->next) {
		DbusmenuMenuitem *child = l->data;
		if (unity_quicklist_handler_menuitem_is_progress_item (child))
			dbusmenu_menuitem_property_set_bool(child,
	                                    DBUSMENU_MENUITEM_PROP_VISIBLE, show);
	}
}

static void
progress_ui_handler_update_unity_launcher_entry (NemoProgressUIHandler *self,
                                                 NemoProgressInfo *info,
                                                 UnityLauncherEntry *entry)
{
	g_return_if_fail (self);
	g_return_if_fail (entry);

	if (self->priv->active_infos > 0) {
		unity_launcher_entry_set_progress_visible (entry, TRUE);
		progress_ui_handler_show_unity_quicklist (self, entry, TRUE);
		progress_ui_handler_unity_progress_changed (NULL, self);

		if (self->priv->active_infos > 1) {
			unity_launcher_entry_set_count (entry, self->priv->active_infos);
			unity_launcher_entry_set_count_visible (entry, TRUE);
		} else {
			unity_launcher_entry_set_count_visible (entry, FALSE);
		}
	} else {
		unity_launcher_entry_set_progress_visible (entry, FALSE);
		unity_launcher_entry_set_progress (entry, 0.0);
		unity_launcher_entry_set_count_visible (entry, FALSE);
		progress_ui_handler_show_unity_quicklist (self, entry, FALSE);
		GCancellable *pc = nemo_progress_info_get_cancellable (info);

		if (!g_cancellable_is_cancelled (pc)) {
			unity_launcher_entry_set_urgent (entry, TRUE);

			g_timeout_add_seconds (2, (GSourceFunc)
				               progress_ui_handler_disable_unity_urgency,
					       entry);
		}
	}
}

static void
progress_ui_handler_update_unity_launcher (NemoProgressUIHandler *self,
                                           NemoProgressInfo *info,
                                           gboolean added)
{
	g_return_if_fail (self);
	GList *l;

	if (!self->priv->unity_quicklist_handler) {
		self->priv->unity_quicklist_handler = unity_quicklist_handler_get_singleton ();
		if (!self->priv->unity_quicklist_handler)
			return;

		progress_ui_handler_build_unity_quicklist (self);
	}

	for (l = unity_quicklist_get_launcher_entries (self->priv->unity_quicklist_handler); l; l = l->next) {
		UnityLauncherEntry *entry = l->data;
		progress_ui_handler_update_unity_launcher_entry (self, info, entry);
	}

	if (added) {
		g_signal_connect (info, "progress-changed",
				  (GCallback) progress_ui_handler_unity_progress_changed,
				  self);
	}
}
#endif

static gboolean
progress_window_delete_event (GtkWidget *widget,
			      GdkEvent *event,
			      NemoProgressUIHandler *self)
{
    gtk_widget_hide (widget);

    self->priv->should_show_status_icon = TRUE;
    progress_ui_handler_update_status_icon (self);

    return TRUE;
}

static void
progress_ui_handler_ensure_window (NemoProgressUIHandler *self)
{
	GtkWidget *vbox, *progress_window;
	const gchar *desktop_environment = g_getenv ("DESKTOP_SESSION");
	
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
				"system-run");
	if ((!g_strcmp0(desktop_environment, "ubuntu")) ||
	       (!g_strcmp0(desktop_environment, "ubuntu-2d")))
	    gtk_window_set_skip_taskbar_hint (GTK_WINDOW (progress_window),
				TRUE);

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

	complete_notification = notify_notification_new (_("File Operations"),
							 _("All file operations have been successfully completed"),
							 NULL);
	notify_notification_show (complete_notification, NULL);

	g_object_unref (complete_notification);
}

static void
progress_ui_handler_hide_status (NemoProgressUIHandler *self)
{
	if (self->priv->status_icon != NULL) {
        self->priv->should_show_status_icon = FALSE;
		gtk_status_icon_set_visible (self->priv->status_icon, FALSE);
	}
}

static void
progress_info_finished_cb (NemoProgressInfo *info,
			   NemoProgressUIHandler *self)
{
	self->priv->active_infos--;
	self->priv->infos = g_list_remove (self->priv->infos, info);

	if (self->priv->active_infos > 0) {
		if (!gtk_widget_get_visible (self->priv->progress_window)) {
			progress_ui_handler_update_status_icon (self);
		}
	} else {
		if (gtk_widget_get_visible (self->priv->progress_window)) {
			gtk_widget_hide (self->priv->progress_window);
		} else {
			progress_ui_handler_hide_status (self);
			progress_ui_handler_show_complete_notification (self);
		}
	}

#ifdef HAVE_UNITY
	progress_ui_handler_update_unity_launcher (self, info, FALSE);
#endif
}

static void
progress_info_changed_cb (NemoProgressInfo *info,
			   NemoProgressUIHandler *self)
{	
	if (g_list_length(self->priv->infos) > 0) {
        NemoProgressInfo *first_info = (NemoProgressInfo *) g_list_first(self->priv->infos)->data;
        GList *l;
        double progress = 0.0;
        int i = 0;
        for (l = self->priv->infos; l != NULL; l = l->next) {
            progress = (progress + nemo_progress_info_get_progress (l->data)) / (double) ++i;
        }
        if (progress > 0) {
            int iprogress = progress * 100;
            gchar *str = g_strdup_printf (_("%d%% %s"), iprogress, nemo_progress_info_get_status(first_info));
            gtk_window_set_title (GTK_WINDOW (self->priv->progress_window), str);
            g_free (str);
            self->priv->active_percent = iprogress;
            progress_ui_handler_update_status_icon (self);
        }
        else {
            gtk_window_set_title (GTK_WINDOW (self->priv->progress_window), nemo_progress_info_get_status(first_info)); 
        }
    } 
}

static void
handle_new_progress_info (NemoProgressUIHandler *self,
			  NemoProgressInfo *info)
{
	self->priv->infos = g_list_append (self->priv->infos, info);	
	
	g_signal_connect (info, "finished",
			  G_CALLBACK (progress_info_finished_cb), self);
			  
	g_signal_connect (info, "progress-changed",
			  G_CALLBACK (progress_info_changed_cb), self);

	self->priv->active_infos++;

	if (self->priv->active_infos == 1) {
		/* this is the only active operation, present the window */
		progress_ui_handler_add_to_window (self, info);
		gtk_window_present (GTK_WINDOW (self->priv->progress_window));
		gtk_window_set_title (GTK_WINDOW (self->priv->progress_window), nemo_progress_info_get_details(info));
		gtk_window_set_icon_name (GTK_WINDOW (self->priv->progress_window), "system-run");			     
	} else {
		progress_ui_handler_add_to_window (self, info);
		progress_ui_handler_update_status_icon (self);
	}

#ifdef HAVE_UNITY
	progress_ui_handler_update_unity_launcher (self, info, TRUE);
#endif
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

static void
nemo_progress_ui_handler_init (NemoProgressUIHandler *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_PROGRESS_UI_HANDLER,
						  NemoProgressUIHandlerPriv);

	self->priv->manager = nemo_progress_info_manager_new ();
	g_signal_connect (self->priv->manager, "new-progress-info",
			  G_CALLBACK (new_progress_info_cb), self);
    self->priv->should_show_status_icon = FALSE;
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
