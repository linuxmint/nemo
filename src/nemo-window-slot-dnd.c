/*
 * nemo-window-slot-dnd.c - Handle DnD for widgets acting as
 * NemoWindowSlot proxies
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 * 	    Ettore Perazzoli <ettore@gnu.org>
 */

#include <config.h>

#include "nemo-notebook.h"
#include "nemo-view-dnd.h"
#include "nemo-window-slot-dnd.h"

typedef struct {
  gboolean have_data;
  gboolean have_valid_data;

  gboolean drop_occured;

  unsigned int info;
  union {
    GList *selection_list;
    GList *uri_list;
    char *netscape_url;
    GtkSelectionData *selection_data;
  } data;

  NemoFile *target_file;
  NemoWindowSlot *target_slot;
  GtkWidget *widget;

  gboolean is_notebook;
  guint switch_location_timer;
} NemoDragSlotProxyInfo;

static void
switch_tab (NemoDragSlotProxyInfo *drag_info)
{
  GtkWidget *notebook, *slot;
  gint idx, n_pages;

  if (drag_info->target_slot == NULL) {
    return;
  }

  notebook = gtk_widget_get_ancestor (GTK_WIDGET (drag_info->target_slot), NEMO_TYPE_NOTEBOOK);
  n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));

  for (idx = 0; idx < n_pages; idx++)
    {
      slot = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), idx);
      if (NEMO_WINDOW_SLOT (slot) == drag_info->target_slot)
        {
          gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), idx);
          break;
        }
    }
}

static void
switch_location (NemoDragSlotProxyInfo *drag_info)
{
  GFile *location;
  GFile *current_location;
  NemoWindowSlot *target_slot;
  GtkWidget *window;

  if (drag_info->target_file == NULL) {
    return;
  }

  window = gtk_widget_get_toplevel (drag_info->widget);
  g_assert (NEMO_IS_WINDOW (window));

  target_slot = nemo_window_get_active_slot (NEMO_WINDOW (window));

  current_location = nemo_window_slot_get_location (target_slot);
  location = nemo_file_get_location (drag_info->target_file);
  if (! (current_location != NULL && g_file_equal (location, current_location))) {
	nemo_window_slot_open_location (target_slot, location, 0);
  }
  g_object_unref (location);
}

static gboolean
slot_proxy_switch_location_timer (gpointer user_data)
{
  NemoDragSlotProxyInfo *drag_info = user_data;

  drag_info->switch_location_timer = 0;

  if (drag_info->is_notebook)
    switch_tab (drag_info);
  else
    switch_location (drag_info);

  return FALSE;
}

static void
slot_proxy_check_switch_location_timer (NemoDragSlotProxyInfo *drag_info,
                                        GtkWidget *widget)
{
  GtkSettings *settings;
  guint timeout;

  if (drag_info->switch_location_timer)
    return;

  settings = gtk_widget_get_settings (widget);
  g_object_get (settings, "gtk-timeout-expand", &timeout, NULL);

  drag_info->switch_location_timer =
    gdk_threads_add_timeout (timeout,
                             slot_proxy_switch_location_timer,
                             drag_info);
}

static void
slot_proxy_remove_switch_location_timer (NemoDragSlotProxyInfo *drag_info)
{
  if (drag_info->switch_location_timer != 0)
    {
      g_source_remove (drag_info->switch_location_timer);
      drag_info->switch_location_timer = 0;
    }
}

static gboolean
slot_proxy_drag_motion (GtkWidget          *widget,
			GdkDragContext     *context,
			int                 x,
			int                 y,
			unsigned int        time,
			gpointer            user_data)
{
  NemoDragSlotProxyInfo *drag_info;
  NemoWindowSlot *target_slot;
  GtkWidget *window;
  GdkAtom target;
  int action;
  char *target_uri;
  gboolean valid_text_drag;
  gboolean valid_xds_drag;

  drag_info = user_data;

  action = 0;
  valid_text_drag = FALSE;
  valid_xds_drag = FALSE;

  if (gtk_drag_get_source_widget (context) == widget) {
    goto out;
  }

  window = gtk_widget_get_toplevel (widget);
  g_assert (NEMO_IS_WINDOW (window));

  if (!drag_info->have_data) {
    target = gtk_drag_dest_find_target (widget, context, NULL);

    if (target == GDK_NONE) {
      goto out;
    }

    gtk_drag_get_data (widget, context, target, time);
  }

  target_uri = NULL;
  if (drag_info->target_file != NULL) {
    target_uri = nemo_file_get_uri (drag_info->target_file);
  } else {
    if (drag_info->target_slot != NULL) {
      target_slot = drag_info->target_slot;
    } else {
      target_slot = nemo_window_get_active_slot (NEMO_WINDOW (window));
    }

    if (target_slot != NULL) {
      target_uri = nemo_window_slot_get_current_uri (target_slot);
    }
  }

  if (target_uri != NULL) {
    NemoFile *file;
    gboolean can;
    file = nemo_file_get_existing_by_uri (target_uri);
    can = nemo_file_can_write (file);
    g_object_unref (file);
    if (!can) {
      action = 0;
      goto out;
    }
  }

  if (drag_info->have_data &&
      drag_info->have_valid_data) {
    if (drag_info->info == NEMO_ICON_DND_GNOME_ICON_LIST) {
      nemo_drag_default_drop_action_for_icons (context, target_uri,
                                                   drag_info->data.selection_list,
                                                   &action);
    } else if (drag_info->info == NEMO_ICON_DND_URI_LIST) {
      action = nemo_drag_default_drop_action_for_uri_list (context, target_uri);
    } else if (drag_info->info == NEMO_ICON_DND_NETSCAPE_URL) {
      action = nemo_drag_default_drop_action_for_netscape_url (context);
    } else if (drag_info->info == NEMO_ICON_DND_TEXT) {
      valid_text_drag = TRUE;
    } else if (drag_info->info == NEMO_ICON_DND_XDNDDIRECTSAVE ||
               drag_info->info == NEMO_ICON_DND_RAW) {
      valid_xds_drag = TRUE;
    }
  }

  g_free (target_uri);

 out:
  if (action != 0 || valid_text_drag || valid_xds_drag) {
    gtk_drag_highlight (widget);
    slot_proxy_check_switch_location_timer (drag_info, widget);
  } else {
    gtk_drag_unhighlight (widget);
    slot_proxy_remove_switch_location_timer (drag_info);
  }

  gdk_drag_status (context, action, time);

  return TRUE;
}

static void
drag_info_free (gpointer user_data)
{
  NemoDragSlotProxyInfo *drag_info = user_data;

  g_clear_object (&drag_info->target_file);
  g_clear_object (&drag_info->target_slot);

  g_slice_free (NemoDragSlotProxyInfo, drag_info);
}

static void
drag_info_clear (NemoDragSlotProxyInfo *drag_info)
{
  slot_proxy_remove_switch_location_timer (drag_info);

  if (!drag_info->have_data) {
    goto out;
  }

  if (drag_info->info == NEMO_ICON_DND_GNOME_ICON_LIST) {
    nemo_drag_destroy_selection_list (drag_info->data.selection_list);
  } else if (drag_info->info == NEMO_ICON_DND_URI_LIST) {
    g_list_free (drag_info->data.uri_list);
  } else if (drag_info->info == NEMO_ICON_DND_NETSCAPE_URL) {
    g_free (drag_info->data.netscape_url);
  } else if (drag_info->info == NEMO_ICON_DND_TEXT ||
             drag_info->info == NEMO_ICON_DND_XDNDDIRECTSAVE ||
             drag_info->info == NEMO_ICON_DND_RAW) {
    if (drag_info->data.selection_data != NULL) {
      gtk_selection_data_free (drag_info->data.selection_data);
    }
  }

 out:
  drag_info->have_data = FALSE;
  drag_info->have_valid_data = FALSE;

  drag_info->drop_occured = FALSE;
}

static void
slot_proxy_drag_leave (GtkWidget          *widget,
		       GdkDragContext     *context,
		       unsigned int        time,
		       gpointer            user_data)
{
    NemoDragSlotProxyInfo *drag_info;

    drag_info = user_data;

    gtk_drag_unhighlight (widget);

    drag_info_clear (drag_info);
}

static gboolean
slot_proxy_drag_drop (GtkWidget          *widget,
		      GdkDragContext     *context,
		      int                 x,
		      int                 y,
		      unsigned int        time,
		      gpointer            user_data)
{
  GdkAtom target;
  NemoDragSlotProxyInfo *drag_info;

  drag_info = user_data;
  g_assert (!drag_info->have_data);

  drag_info->drop_occured = TRUE;

  target = gtk_drag_dest_find_target (widget, context, NULL);
  gtk_drag_get_data (widget, context, target, time);

  return TRUE;
}


static void
slot_proxy_handle_drop (GtkWidget                *widget,
			GdkDragContext           *context,
			unsigned int              time,
			NemoDragSlotProxyInfo *drag_info)
{
  GtkWidget *window;
  NemoWindowSlot *target_slot;
  NemoView *target_view;
  char *target_uri;
  GList *uri_list;

  if (!drag_info->have_data ||
      !drag_info->have_valid_data) {
    gtk_drag_finish (context, FALSE, FALSE, time);
    drag_info_clear (drag_info);
    return;
  }

  window = gtk_widget_get_toplevel (widget);
  g_assert (NEMO_IS_WINDOW (window));

  if (drag_info->target_slot != NULL) {
    target_slot = drag_info->target_slot;
  } else {
    target_slot = nemo_window_get_active_slot (NEMO_WINDOW (window));
  }

  target_uri = NULL;
  if (drag_info->target_file != NULL) {
    target_uri = nemo_file_get_uri (drag_info->target_file);
  } else if (target_slot != NULL) {
    target_uri = nemo_window_slot_get_current_uri (target_slot);
  }

  target_view = NULL;
  if (target_slot != NULL) {
    target_view = nemo_window_slot_get_current_view (target_slot);
  }

  if (target_slot != NULL && target_view != NULL) {
    if (drag_info->info == NEMO_ICON_DND_GNOME_ICON_LIST) {
      uri_list = nemo_drag_uri_list_from_selection_list (drag_info->data.selection_list);
      g_assert (uri_list != NULL);

      nemo_view_drop_proxy_received_uris (target_view,
                                              uri_list,
                                              target_uri,
                                              gdk_drag_context_get_selected_action (context));
      g_list_free_full (uri_list, g_free);
    } else if (drag_info->info == NEMO_ICON_DND_URI_LIST) {
      nemo_view_drop_proxy_received_uris (target_view,
                                              drag_info->data.uri_list,
                                              target_uri,
                                              gdk_drag_context_get_selected_action (context));
    } if (drag_info->info == NEMO_ICON_DND_NETSCAPE_URL) {
      nemo_view_handle_netscape_url_drop (target_view,
                                              drag_info->data.netscape_url,
                                              target_uri,
                                              gdk_drag_context_get_selected_action (context),
                                              0, 0);
    }


    gtk_drag_finish (context, TRUE, FALSE, time);
  } else {
    gtk_drag_finish (context, FALSE, FALSE, time);
  }

  g_free (target_uri);

  drag_info_clear (drag_info);
}

static void
slot_proxy_drag_data_received (GtkWidget          *widget,
			       GdkDragContext     *context,
			       int                 x,
			       int                 y,
			       GtkSelectionData   *data,
			       unsigned int        info,
			       unsigned int        time,
			       gpointer            user_data)
{
  NemoDragSlotProxyInfo *drag_info;
  char **uris;

  drag_info = user_data;

  g_assert (!drag_info->have_data);

  drag_info->have_data = TRUE;
  drag_info->info = info;

  if (gtk_selection_data_get_length (data) < 0) {
    drag_info->have_valid_data = FALSE;
    return;
  }

  if (info == NEMO_ICON_DND_GNOME_ICON_LIST) {
    drag_info->data.selection_list = nemo_drag_build_selection_list (data);

    drag_info->have_valid_data = drag_info->data.selection_list != NULL;
  } else if (info == NEMO_ICON_DND_URI_LIST) {
    uris = gtk_selection_data_get_uris (data);
    drag_info->data.uri_list = nemo_drag_uri_list_from_array ((const char **) uris);
    g_strfreev (uris);

    drag_info->have_valid_data = drag_info->data.uri_list != NULL;
  } else if (info == NEMO_ICON_DND_NETSCAPE_URL) {
    drag_info->data.netscape_url = g_strdup ((char *) gtk_selection_data_get_data (data));

    drag_info->have_valid_data = drag_info->data.netscape_url != NULL;
  } else if (info == NEMO_ICON_DND_TEXT ||
             info == NEMO_ICON_DND_XDNDDIRECTSAVE ||
             info == NEMO_ICON_DND_RAW) {
    drag_info->data.selection_data = gtk_selection_data_copy (data);
    drag_info->have_valid_data = drag_info->data.selection_data != NULL;
  }

  if (drag_info->drop_occured) {
    slot_proxy_handle_drop (widget, context, time, drag_info);
  }
}

void
nemo_drag_slot_proxy_init (GtkWidget *widget,
                               NemoFile *target_file,
                               NemoWindowSlot *target_slot)
{
  NemoDragSlotProxyInfo *drag_info;

  const GtkTargetEntry targets[] = {
    { NEMO_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NEMO_ICON_DND_GNOME_ICON_LIST },
    { NEMO_ICON_DND_NETSCAPE_URL_TYPE, 0, NEMO_ICON_DND_NETSCAPE_URL },
    { NEMO_ICON_DND_XDNDDIRECTSAVE_TYPE, 0, NEMO_ICON_DND_XDNDDIRECTSAVE }, /* XDS Protocol Type */
    { NEMO_ICON_DND_RAW_TYPE, 0, NEMO_ICON_DND_RAW }
  };
  GtkTargetList *target_list;

  g_assert (GTK_IS_WIDGET (widget));

  drag_info = g_slice_new0 (NemoDragSlotProxyInfo);

  g_object_set_data_full (G_OBJECT (widget), "drag-slot-proxy-data", drag_info,
                          drag_info_free);

  drag_info->is_notebook = (g_object_get_data (G_OBJECT (widget), "nemo-notebook-tab") != NULL);

  if (target_file != NULL)
    drag_info->target_file = g_object_ref (target_file);

  if (target_slot != NULL)
    drag_info->target_slot = g_object_ref (target_slot);

  drag_info->widget = widget;

  gtk_drag_dest_set (widget, 0,
                     NULL, 0,
                     GDK_ACTION_MOVE |
                     GDK_ACTION_COPY |
                     GDK_ACTION_LINK |
                     GDK_ACTION_ASK);

  target_list = gtk_target_list_new (targets, G_N_ELEMENTS (targets));
  gtk_target_list_add_uri_targets (target_list, NEMO_ICON_DND_URI_LIST);
  gtk_target_list_add_text_targets (target_list, NEMO_ICON_DND_TEXT);
  gtk_drag_dest_set_target_list (widget, target_list);
  gtk_target_list_unref (target_list);

  g_signal_connect (widget, "drag-motion",
                    G_CALLBACK (slot_proxy_drag_motion),
                    drag_info);
  g_signal_connect (widget, "drag-drop",
                    G_CALLBACK (slot_proxy_drag_drop),
                    drag_info);
  g_signal_connect (widget, "drag-data-received",
                    G_CALLBACK (slot_proxy_drag_data_received),
                    drag_info);
  g_signal_connect (widget, "drag-leave",
                    G_CALLBACK (slot_proxy_drag_leave),
                    drag_info);
}
