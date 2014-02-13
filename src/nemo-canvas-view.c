/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-canvas-view.c - implementation of canvas view of directory.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nemo-canvas-view.h"

#include "nemo-actions.h"
#include "nemo-canvas-view-container.h"
#include "nemo-desktop-canvas-view.h"
#include "nemo-error-reporting.h"
#include "nemo-view-dnd.h"
#include "nemo-view-factory.h"
#include "nemo-window.h"

#include <stdlib.h>
#include <eel/eel-vfs-extensions.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnemo-private/nemo-clipboard-monitor.h>
#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-file-dnd.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-canvas-container.h>
#include <libnemo-private/nemo-canvas-dnd.h>
#include <libnemo-private/nemo-link.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-desktop-icon-file.h>

#define DEBUG_FLAG NEMO_DEBUG_CANVAS_VIEW
#include <libnemo-private/nemo-debug.h>

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define POPUP_PATH_CANVAS_APPEARANCE		"/selection/Canvas Appearance Items"

enum 
{
	PROP_COMPACT = 1,
	PROP_SUPPORTS_AUTO_LAYOUT,
	PROP_SUPPORTS_SCALING,
	PROP_SUPPORTS_KEEP_ALIGNED,
	PROP_SUPPORTS_MANUAL_LAYOUT,
	PROP_SUPPORTS_LABELS_BESIDE_ICONS,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

typedef struct {
	const NemoFileSortType sort_type;
	const char *metadata_text;
	const char *action;
	const char *menu_label;
	const char *menu_hint;
} SortCriterion;

typedef enum {
	MENU_ITEM_TYPE_STANDARD,
	MENU_ITEM_TYPE_CHECK,
	MENU_ITEM_TYPE_RADIO,
	MENU_ITEM_TYPE_TREE
} MenuItemType;

struct NemoCanvasViewDetails
{
	GList *icons_not_positioned;

	guint react_to_canvas_change_idle_id;

	const SortCriterion *sort;
	gboolean sort_reversed;

	GtkActionGroup *canvas_action_group;
	guint canvas_merge_id;
	
	gboolean filter_by_screen;
	int num_screens;

	gboolean compact;

	gulong clipboard_handler_id;

	GtkWidget *canvas_container;

	gboolean supports_auto_layout;
	gboolean supports_manual_layout;
	gboolean supports_scaling;
	gboolean supports_keep_aligned;
	gboolean supports_labels_beside_icons;
};


/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] = {
	{
		NEMO_FILE_SORT_BY_DISPLAY_NAME,
		"name",
		"Sort by Name",
		N_("by _Name"),
		N_("Keep icons sorted by name in rows")
	},
	{
		NEMO_FILE_SORT_BY_SIZE,
		"size",
		"Sort by Size",
		N_("by _Size"),
		N_("Keep icons sorted by size in rows")
	},
	{
		NEMO_FILE_SORT_BY_TYPE,
		"type",
		"Sort by Type",
		N_("by _Type"),
		N_("Keep icons sorted by type in rows")
	},
	{
		NEMO_FILE_SORT_BY_MTIME,
		"modification date",
		"Sort by Modification Date",
		N_("by Modification _Date"),
		N_("Keep icons sorted by modification date in rows")
	},
	{
		NEMO_FILE_SORT_BY_TRASHED_TIME,
		"trashed",
		"Sort by Trash Time",
		N_("by T_rash Time"),
		N_("Keep icons sorted by trash time in rows")
	}
};

static void                 nemo_canvas_view_set_directory_sort_by        (NemoCanvasView           *canvas_view,
									     NemoFile         *file,
									     const char           *sort_by);
static void                 nemo_canvas_view_set_zoom_level               (NemoCanvasView           *view,
									     NemoZoomLevel     new_level,
									     gboolean              always_emit);
static void                 nemo_canvas_view_update_click_mode            (NemoCanvasView           *canvas_view);
static void                 nemo_canvas_view_set_directory_tighter_layout (NemoCanvasView           *canvas_view,
                                        NemoFile         *file,
                                        gboolean              tighter_layout);
static gboolean             nemo_canvas_view_supports_scaling	      (NemoCanvasView           *canvas_view);
static void                 nemo_canvas_view_reveal_selection       (NemoView               *view);
static const SortCriterion *get_sort_criterion_by_sort_type           (NemoFileSortType  sort_type);
static void                 set_sort_criterion_by_sort_type           (NemoCanvasView           *canvas_view,
								       NemoFileSortType  sort_type);
static gboolean             set_sort_reversed                         (NemoCanvasView     *canvas_view,
								       gboolean              new_value,
								       gboolean              set_metadata);
static void                 switch_to_manual_layout                   (NemoCanvasView     *view);
static void                 update_layout_menus                       (NemoCanvasView     *view);
static NemoFileSortType get_default_sort_order                    (NemoFile         *file,
								       gboolean             *reversed);
static void                 nemo_canvas_view_clear                  (NemoView         *view);

G_DEFINE_TYPE (NemoCanvasView, nemo_canvas_view, NEMO_TYPE_VIEW);

static void
nemo_canvas_view_destroy (GtkWidget *object)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (object);

	nemo_canvas_view_clear (NEMO_VIEW (object));

        if (canvas_view->details->react_to_canvas_change_idle_id != 0) {
                g_source_remove (canvas_view->details->react_to_canvas_change_idle_id);
		canvas_view->details->react_to_canvas_change_idle_id = 0;
        }

	if (canvas_view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nemo_clipboard_monitor_get (),
					     canvas_view->details->clipboard_handler_id);
		canvas_view->details->clipboard_handler_id = 0;
	}

	if (canvas_view->details->icons_not_positioned) {
		nemo_file_list_free (canvas_view->details->icons_not_positioned);
		canvas_view->details->icons_not_positioned = NULL;
	}

	GTK_WIDGET_CLASS (nemo_canvas_view_parent_class)->destroy (object);
}

static NemoCanvasContainer *
get_canvas_container (NemoCanvasView *canvas_view)
{
	return NEMO_CANVAS_CONTAINER (canvas_view->details->canvas_container);
}

NemoCanvasContainer *
nemo_canvas_view_get_canvas_container (NemoCanvasView *canvas_view)
{
	return get_canvas_container (canvas_view);
}

static gboolean
nemo_canvas_view_supports_manual_layout (NemoCanvasView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return !nemo_canvas_view_is_compact (view) && view->details->supports_manual_layout;
}

static gboolean
get_stored_icon_position_callback (NemoCanvasContainer *container,
				   NemoFile *file,
				   NemoCanvasPosition *position,
				   NemoCanvasView *canvas_view)
{
	char *position_string, *scale_string;
	gboolean position_good;
	char c;

	g_assert (NEMO_IS_CANVAS_CONTAINER (container));
	g_assert (NEMO_IS_FILE (file));
	g_assert (position != NULL);
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));

	if (!nemo_canvas_view_supports_manual_layout (canvas_view)) {
		return FALSE;
	}

	/* Get the current position of this canvas from the metadata. */
	position_string = nemo_file_get_metadata
		(file, NEMO_METADATA_KEY_ICON_POSITION, "");
	position_good = sscanf
		(position_string, " %d , %d %c",
		 &position->x, &position->y, &c) == 2;
	g_free (position_string);

	/* If it is the desktop directory, maybe the gnome-libs metadata has information about it */

	/* Disable scaling if not on the desktop */
	if (nemo_canvas_view_supports_scaling (canvas_view)) {
		/* Get the scale of the canvas from the metadata. */
		scale_string = nemo_file_get_metadata
			(file, NEMO_METADATA_KEY_ICON_SCALE, "1");
		position->scale = g_ascii_strtod (scale_string, NULL);
		if (errno != 0) {
			position->scale = 1.0;
		}

		g_free (scale_string);
	} else {
		position->scale = 1.0;
	}
	
	return position_good;
}

static void
real_set_sort_criterion (NemoCanvasView *canvas_view,
                         const SortCriterion *sort,
                         gboolean clear,
			 gboolean set_metadata)
{
	NemoFile *file;

	file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));

	if (clear) {
		nemo_file_set_metadata (file,
					    NEMO_METADATA_KEY_ICON_VIEW_SORT_BY, NULL, NULL);
		nemo_file_set_metadata (file,
					    NEMO_METADATA_KEY_ICON_VIEW_SORT_REVERSED, NULL, NULL);
		canvas_view->details->sort =
			get_sort_criterion_by_sort_type	(get_default_sort_order
							 (file, &canvas_view->details->sort_reversed));
	} else if (set_metadata) {
		/* Store the new sort setting. */
		nemo_canvas_view_set_directory_sort_by (canvas_view,
						    file,
						    sort->metadata_text);
	}

	/* Update the layout menus to match the new sort setting. */
	update_layout_menus (canvas_view);
}

static void
set_sort_criterion (NemoCanvasView *canvas_view,
		    const SortCriterion *sort,
		    gboolean set_metadata)
{
	if (sort == NULL ||
	    canvas_view->details->sort == sort) {
		return;
	}

	canvas_view->details->sort = sort;

        real_set_sort_criterion (canvas_view, sort, FALSE, set_metadata);
}

static void
clear_sort_criterion (NemoCanvasView *canvas_view)
{
	real_set_sort_criterion (canvas_view, NULL, TRUE, TRUE);
}

static void
action_stretch_callback (GtkAction *action,
			 gpointer callback_data)
{
	g_assert (NEMO_IS_CANVAS_VIEW (callback_data));

	nemo_canvas_container_show_stretch_handles
		(get_canvas_container (NEMO_CANVAS_VIEW (callback_data)));
}

static void
action_unstretch_callback (GtkAction *action,
			   gpointer callback_data)
{
	g_assert (NEMO_IS_CANVAS_VIEW (callback_data));

	nemo_canvas_container_unstretch
		(get_canvas_container (NEMO_CANVAS_VIEW (callback_data)));
}

static void
nemo_canvas_view_clean_up (NemoCanvasView *canvas_view)
{
	NemoCanvasContainer *canvas_container;
	gboolean saved_sort_reversed;

	canvas_container = get_canvas_container (canvas_view);

	/* Hardwire Clean Up to always be by name, in forward order */
	saved_sort_reversed = canvas_view->details->sort_reversed;
	
	set_sort_reversed (canvas_view, FALSE, FALSE);
	set_sort_criterion (canvas_view, &sort_criteria[0], FALSE);

	nemo_canvas_container_sort (canvas_container);
	nemo_canvas_container_freeze_icon_positions (canvas_container);

	set_sort_reversed (canvas_view, saved_sort_reversed, FALSE);
}

static void
action_clean_up_callback (GtkAction *action, gpointer callback_data)
{
	nemo_canvas_view_clean_up (NEMO_CANVAS_VIEW (callback_data));
}

static void
set_tighter_layout (NemoCanvasView *canvas_view, gboolean new_value)
{
   nemo_canvas_view_set_directory_tighter_layout (canvas_view,
                          nemo_view_get_directory_as_file 
                          (NEMO_VIEW (canvas_view)),
                          new_value);
   nemo_canvas_container_set_tighter_layout (get_canvas_container (canvas_view),
                           new_value); 
}

static void
action_tighter_layout_callback (GtkAction *action,
               gpointer user_data)
{
   g_assert (NEMO_IS_CANVAS_VIEW (user_data));

   set_tighter_layout (NEMO_CANVAS_VIEW (user_data),
               gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

static gboolean
nemo_canvas_view_using_auto_layout (NemoCanvasView *canvas_view)
{
	return nemo_canvas_container_is_auto_layout 
		(get_canvas_container (canvas_view));
}

static gboolean
nemo_canvas_view_using_tighter_layout (NemoCanvasView *icon_view)
{
   return nemo_canvas_container_is_tighter_layout
       (get_canvas_container (icon_view));
}

static void
action_sort_radio_callback (GtkAction *action,
			    GtkRadioAction *current,
			    NemoCanvasView *view)
{
	NemoFileSortType sort_type;
	
	sort_type = gtk_radio_action_get_current_value (current);
	
	/* Note that id might be a toggle item.
	 * Ignore non-sort ids so that they don't cause sorting.
	 */
	if (sort_type == NEMO_FILE_SORT_NONE) {
		switch_to_manual_layout (view);
	} else {
		set_sort_criterion_by_sort_type (view, sort_type);
	}
}

static void
list_covers (NemoCanvasIconData *data, gpointer callback_data)
{
	GSList **file_list;

	file_list = callback_data;

	*file_list = g_slist_prepend (*file_list, data);
}

static void
unref_cover (NemoCanvasIconData *data, gpointer callback_data)
{
	nemo_file_unref (NEMO_FILE (data));
}

static void
nemo_canvas_view_clear (NemoView *view)
{
	NemoCanvasContainer *canvas_container;
	GSList *file_list;
	
	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NEMO_CANVAS_VIEW (view));
	if (!canvas_container)
		return;

	/* Clear away the existing icons. */
	file_list = NULL;
	nemo_canvas_container_for_each (canvas_container, list_covers, &file_list);
	nemo_canvas_container_clear (canvas_container);
	g_slist_foreach (file_list, (GFunc)unref_cover, NULL);
	g_slist_free (file_list);
}

static gboolean
should_show_file_on_screen (NemoView *view, NemoFile *file)
{
	char *screen_string;
	int screen_num;
	NemoCanvasView *canvas_view;
	GdkScreen *screen;

	canvas_view = NEMO_CANVAS_VIEW (view);

	if (!nemo_view_should_show_file (view, file)) {
		return FALSE;
	}
	
	/* Get the screen for this canvas from the metadata. */
	screen_string = nemo_file_get_metadata
		(file, NEMO_METADATA_KEY_SCREEN, "0");
	screen_num = atoi (screen_string);
	g_free (screen_string);
	screen = gtk_widget_get_screen (GTK_WIDGET (view));

	if (screen_num != gdk_screen_get_number (screen) &&
	    (screen_num < canvas_view->details->num_screens ||
	     gdk_screen_get_number (screen) > 0)) {
		return FALSE;
	}

	return TRUE;
}

static void
nemo_canvas_view_remove_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	NemoCanvasView *canvas_view;

	/* This used to assert that 'directory == nemo_view_get_model (view)', but that
	 * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
	 * It seems that somehow we get a files_changed event sent to the view from a directory
	 * that isn't the model, but the code disables the monitor and signal callback handlers when
	 * changing directories. Maybe we can get some more information when this happens.
	 * Further discussion in bug #368178.
	 */
	if (directory != nemo_view_get_model (view)) {
		char *file_uri, *dir_uri, *model_uri;
		file_uri = nemo_file_get_uri (file);
		dir_uri = nemo_directory_get_uri (directory);
		model_uri = nemo_directory_get_uri (nemo_view_get_model (view));
		g_warning ("nemo_canvas_view_remove_file() - directory not canvas view model, shouldn't happen.\n"
			   "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
			   "If you see this, please add this info to http://bugzilla.gnome.org/show_bug.cgi?id=368178",
			   file, file_uri, directory, dir_uri, nemo_view_get_model (view), model_uri, nemo_view_get_loading (view));
		g_free (file_uri);
		g_free (dir_uri);
		g_free (model_uri);
	}
	
	canvas_view = NEMO_CANVAS_VIEW (view);

	if (nemo_canvas_container_remove (get_canvas_container (canvas_view),
					      NEMO_CANVAS_ICON_DATA (file))) {
		nemo_file_unref (file);
	}
}

static void
nemo_canvas_view_add_file (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	NemoCanvasView *canvas_view;
	NemoCanvasContainer *canvas_container;

	g_assert (directory == nemo_view_get_model (view));
	
	canvas_view = NEMO_CANVAS_VIEW (view);
	canvas_container = get_canvas_container (canvas_view);

	if (canvas_view->details->filter_by_screen &&
	    !should_show_file_on_screen (view, file)) {
		return;
	}

	/* Reset scroll region for the first canvas added when loading a directory. */
	if (nemo_view_get_loading (view) && nemo_canvas_container_is_empty (canvas_container)) {
		nemo_canvas_container_reset_scroll_region (canvas_container);
	}

	if (nemo_canvas_container_add (canvas_container,
					 NEMO_CANVAS_ICON_DATA (file))) {
		nemo_file_ref (file);
	}
}

static void
nemo_canvas_view_file_changed (NemoView *view, NemoFile *file, NemoDirectory *directory)
{
	NemoCanvasView *canvas_view;

	g_assert (directory == nemo_view_get_model (view));
	
	g_return_if_fail (view != NULL);
	canvas_view = NEMO_CANVAS_VIEW (view);

	if (!canvas_view->details->filter_by_screen) {
		nemo_canvas_container_request_update
			(get_canvas_container (canvas_view),
			 NEMO_CANVAS_ICON_DATA (file));
		return;
	}
	
	if (!should_show_file_on_screen (view, file)) {
		nemo_canvas_view_remove_file (view, file, directory);
	} else {

		nemo_canvas_container_request_update
			(get_canvas_container (canvas_view),
			 NEMO_CANVAS_ICON_DATA (file));
	}
}

static gboolean
nemo_canvas_view_supports_auto_layout (NemoCanvasView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_auto_layout;
}

static gboolean
nemo_canvas_view_supports_scaling (NemoCanvasView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_scaling;
}

static gboolean
nemo_canvas_view_supports_keep_aligned (NemoCanvasView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_keep_aligned;
}

static gboolean
nemo_canvas_view_supports_labels_beside_icons (NemoCanvasView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_labels_beside_icons;
}

static gboolean
nemo_canvas_view_supports_tighter_layout (NemoCanvasView *view)
{
   return !nemo_canvas_view_is_compact (view);
}

static void
update_layout_menus (NemoCanvasView *view)
{
	gboolean is_auto_layout;
	GtkAction *action;
	const char *action_name;
	NemoFile *file;

	if (view->details->canvas_action_group == NULL) {
		return;
	}

	is_auto_layout = nemo_canvas_view_using_auto_layout (view);
	file = nemo_view_get_directory_as_file (NEMO_VIEW (view));

	if (nemo_canvas_view_supports_auto_layout (view)) {
		/* Mark sort criterion. */
		action_name = is_auto_layout ? view->details->sort->action : NEMO_ACTION_MANUAL_LAYOUT;
		action = gtk_action_group_get_action (view->details->canvas_action_group,
						      action_name);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		action = gtk_action_group_get_action (view->details->canvas_action_group,
                                              NEMO_ACTION_TIGHTER_LAYOUT);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                          nemo_canvas_view_using_tighter_layout (view));
        gtk_action_set_sensitive (action, nemo_canvas_view_supports_tighter_layout (view));
        gtk_action_set_visible (action, nemo_canvas_view_supports_tighter_layout (view));

        action = gtk_action_group_get_action (view->details->canvas_action_group,
                                              NEMO_ACTION_REVERSED_ORDER);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      view->details->sort_reversed);
		gtk_action_set_sensitive (action, is_auto_layout);

		action = gtk_action_group_get_action (view->details->canvas_action_group,
		                                      NEMO_ACTION_SORT_TRASH_TIME);

		if (file != NULL && nemo_file_is_in_trash (file)) {
			gtk_action_set_visible (action, TRUE);
		} else {
			gtk_action_set_visible (action, FALSE);
		}
	}

	action = gtk_action_group_get_action (view->details->canvas_action_group,
					      NEMO_ACTION_MANUAL_LAYOUT);
	gtk_action_set_visible (action,
				nemo_canvas_view_supports_manual_layout (view));

	/* Clean Up is only relevant for manual layout */
	action = gtk_action_group_get_action (view->details->canvas_action_group,
					      NEMO_ACTION_CLEAN_UP);
	gtk_action_set_sensitive (action, !is_auto_layout);	
	gtk_action_set_visible (action,
				nemo_canvas_view_supports_manual_layout (view));

	if (NEMO_IS_DESKTOP_CANVAS_VIEW (view)) {
		gtk_action_set_label (action, _("_Organize Desktop by Name"));
	}

	action = gtk_action_group_get_action (view->details->canvas_action_group,
					      NEMO_ACTION_KEEP_ALIGNED);
	gtk_action_set_visible (action,
				nemo_canvas_view_supports_keep_aligned (view));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nemo_canvas_container_is_keep_aligned (get_canvas_container (view)));
	gtk_action_set_sensitive (action, !is_auto_layout);
}


static char *
nemo_canvas_view_get_directory_sort_by (NemoCanvasView *canvas_view,
					  NemoFile *file)
{
	const SortCriterion *default_sort_criterion;

	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		return g_strdup ("name");
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_val_if_fail (default_sort_criterion != NULL, NULL);

	return nemo_file_get_metadata
		(file, NEMO_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text);
}

static NemoFileSortType
get_default_sort_order (NemoFile *file, gboolean *reversed)
{
	NemoFileSortType retval, default_sort_order;
	gboolean default_sort_in_reverse_order;

	default_sort_order = g_settings_get_enum (nemo_preferences,
						  NEMO_PREFERENCES_DEFAULT_SORT_ORDER);
	default_sort_in_reverse_order = g_settings_get_boolean (nemo_preferences,
								NEMO_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

	retval = nemo_file_get_default_sort_type (file, reversed);

	if (retval == NEMO_FILE_SORT_NONE) {

		if (reversed != NULL) {
			*reversed = default_sort_in_reverse_order;
		}

		retval = CLAMP (default_sort_order, NEMO_FILE_SORT_BY_DISPLAY_NAME,
				NEMO_FILE_SORT_BY_MTIME);
	}

	return retval;
}

static void
nemo_canvas_view_set_directory_sort_by (NemoCanvasView *canvas_view, 
					  NemoFile *file, 
					  const char *sort_by)
{
	const SortCriterion *default_sort_criterion;

	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		return;
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_if_fail (default_sort_criterion != NULL);

	nemo_file_set_metadata
		(file, NEMO_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text,
		 sort_by);
}

static gboolean
nemo_canvas_view_get_directory_sort_reversed (NemoCanvasView *canvas_view,
						NemoFile *file)
{
	gboolean reversed;

	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		return FALSE;
	}

	get_default_sort_order (file, &reversed);
	return nemo_file_get_boolean_metadata
		(file,
		 NEMO_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 reversed);
}

static void
nemo_canvas_view_set_directory_sort_reversed (NemoCanvasView *canvas_view,
						NemoFile *file,
						gboolean sort_reversed)
{
	gboolean reversed;

	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		return;
	}

	get_default_sort_order (file, &reversed);
	nemo_file_set_boolean_metadata
		(file,
		 NEMO_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 reversed, sort_reversed);
}

static gboolean
get_default_directory_keep_aligned (void)
{
	return TRUE;
}

static gboolean
nemo_canvas_view_get_directory_keep_aligned (NemoCanvasView *canvas_view,
					       NemoFile *file)
{
	if (!nemo_canvas_view_supports_keep_aligned (canvas_view)) {
		return FALSE;
	}
	
	return  nemo_file_get_boolean_metadata
		(file,
		 NEMO_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned ());
}

static void
nemo_canvas_view_set_directory_keep_aligned (NemoCanvasView *canvas_view,
					       NemoFile *file,
					       gboolean keep_aligned)
{
	if (!nemo_canvas_view_supports_keep_aligned (canvas_view)) {
		return;
	}

	nemo_file_set_boolean_metadata
		(file, NEMO_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned (),
		 keep_aligned);
}

static gboolean
nemo_canvas_view_get_directory_auto_layout (NemoCanvasView *canvas_view,
					      NemoFile *file)
{
	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		return FALSE;
	}

	if (!nemo_canvas_view_supports_manual_layout (canvas_view)) {
		return TRUE;
	}

	return nemo_file_get_boolean_metadata
		(file, NEMO_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, TRUE);
}

static void
nemo_canvas_view_set_directory_auto_layout (NemoCanvasView *canvas_view,
					      NemoFile *file,
					gboolean auto_layout)
{
	if (!nemo_canvas_view_supports_auto_layout (canvas_view) ||
	    !nemo_canvas_view_supports_manual_layout (canvas_view)) {
		return;
	}

	nemo_file_set_boolean_metadata
		(file, NEMO_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
		 TRUE,
		 auto_layout);
}

/* maintainence of tighter layout boolean */

static gboolean
get_default_directory_tighter_layout (void)
{
    return g_settings_get_boolean (nemo_canvas_view_preferences,
                      NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_USE_TIGHTER_LAYOUT);
}

static gboolean
nemo_canvas_view_get_directory_tighter_layout (NemoCanvasView *canvas_view,
                        NemoFile *file)
{
    if (!nemo_canvas_view_supports_tighter_layout (canvas_view)) {
        return FALSE;
    }

    if (nemo_global_preferences_get_ignore_view_metadata ()) {
        gint t = nemo_window_get_ignore_meta_tighter_layout (nemo_view_get_window (NEMO_VIEW (canvas_view)));
        return t > TIGHTER_NULL ? t == TIGHTER_YES : get_default_directory_tighter_layout ();
    }

    return nemo_file_get_boolean_metadata (file,
                                           NEMO_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
                                           get_default_directory_tighter_layout ());
}

static void
nemo_canvas_view_set_directory_tighter_layout (NemoCanvasView *canvas_view,
                        NemoFile *file,
                        gboolean tighter_layout)
{
    if (!nemo_canvas_view_supports_tighter_layout (canvas_view)) {
        return;
    }

    if (nemo_global_preferences_get_ignore_view_metadata ()) {
        gint t = tighter_layout ? TIGHTER_YES : TIGHTER_NO;
        nemo_window_set_ignore_meta_tighter_layout (nemo_view_get_window (NEMO_VIEW (canvas_view)), t);
    } else {
        nemo_file_set_boolean_metadata (file, NEMO_METADATA_KEY_ICON_VIEW_TIGHTER_LAYOUT,
                                        get_default_directory_tighter_layout (),
                                        tighter_layout);
    }
}

static gboolean
set_sort_reversed (NemoCanvasView *canvas_view,
		   gboolean new_value,
		   gboolean set_metadata)
{
	if (canvas_view->details->sort_reversed == new_value) {
		return FALSE;
	}
	canvas_view->details->sort_reversed = new_value;

	if (set_metadata) {
		/* Store the new sort setting. */
		nemo_canvas_view_set_directory_sort_reversed (canvas_view, nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view)), new_value);
	}
	
	/* Update the layout menus to match the new sort-order setting. */
	update_layout_menus (canvas_view);

	return TRUE;
}

static const SortCriterion *
get_sort_criterion_by_metadata_text (const char *metadata_text)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (strcmp (sort_criteria[i].metadata_text, metadata_text) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_sort_type (NemoFileSortType sort_type)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (sort_type == sort_criteria[i].sort_type) {
			return &sort_criteria[i];
		}
	}

	return &sort_criteria[0];
}

#define DEFAULT_ZOOM_LEVEL(icon_view) canvas_view->details->compact ? default_compact_zoom_level : default_zoom_level

static NemoZoomLevel
get_default_zoom_level (NemoCanvasView *canvas_view)
{
	NemoZoomLevel default_zoom_level, default_compact_zoom_level;

	default_zoom_level = g_settings_get_enum (nemo_canvas_view_preferences,
						  NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_ZOOM_LEVEL);
	default_compact_zoom_level = g_settings_get_enum (nemo_compact_view_preferences,
							  NEMO_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (DEFAULT_ZOOM_LEVEL(canvas_view), NEMO_ZOOM_LEVEL_SMALLEST, NEMO_ZOOM_LEVEL_LARGEST);
}

static void
set_labels_beside_icons (NemoCanvasView *canvas_view)
{
	gboolean labels_beside;

	if (nemo_canvas_view_supports_labels_beside_icons (canvas_view)) {
		labels_beside = nemo_canvas_view_is_compact (canvas_view) ||
			g_settings_get_boolean (nemo_canvas_view_preferences,
						NEMO_PREFERENCES_CANVAS_VIEW_LABELS_BESIDE_ICONS);

		if (labels_beside) {
			nemo_canvas_container_set_label_position
				(get_canvas_container (canvas_view),
				 NEMO_CANVAS_LABEL_POSITION_BESIDE);
		} else {
			nemo_canvas_container_set_label_position
				(get_canvas_container (canvas_view),
				 NEMO_CANVAS_LABEL_POSITION_UNDER);
		}
	}
}

static void
set_columns_same_width (NemoCanvasView *canvas_view)
{
	gboolean all_columns_same_width;

	if (nemo_canvas_view_is_compact (canvas_view)) {
		all_columns_same_width = g_settings_get_boolean (nemo_compact_view_preferences,
								 NEMO_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH);
		nemo_canvas_container_set_all_columns_same_width (get_canvas_container (canvas_view), all_columns_same_width);
	}
}

static void
nemo_canvas_view_begin_loading (NemoView *view)
{
	NemoCanvasView *canvas_view;
	GtkWidget *canvas_container;
	NemoFile *file;
	int level;
	char *sort_name, *uri;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	canvas_view = NEMO_CANVAS_VIEW (view);
	file = nemo_view_get_directory_as_file (view);
	uri = nemo_file_get_uri (file);
	canvas_container = GTK_WIDGET (get_canvas_container (canvas_view));

	nemo_canvas_container_begin_loading (NEMO_CANVAS_CONTAINER (canvas_container));

	nemo_canvas_container_set_allow_moves (NEMO_CANVAS_CONTAINER (canvas_container),
						 !eel_uri_is_search (uri));

	g_free (uri);

	/* Set up the zoom level from the metadata. */
	if (nemo_view_supports_zooming (NEMO_VIEW (canvas_view))) {
        if (nemo_global_preferences_get_ignore_view_metadata () &&
            nemo_window_get_ignore_meta_zoom_level (nemo_view_get_window (NEMO_VIEW (canvas_view))) > -1) {
            level = nemo_window_get_ignore_meta_zoom_level (nemo_view_get_window (NEMO_VIEW (canvas_view)));
        } else {
    		if (canvas_view->details->compact) {
    			level = nemo_file_get_integer_metadata
    				(file, 
    				 NEMO_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL, 
    				 get_default_zoom_level (canvas_view));
    		} else {
    			level = nemo_file_get_integer_metadata
    				(file, 
    				 NEMO_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
    				 get_default_zoom_level (canvas_view));
    		}
        }

		nemo_canvas_view_set_zoom_level (canvas_view, level, TRUE);
	}

	/* Set the sort mode.
	 * It's OK not to resort the icons because the
	 * container doesn't have any icons at this point.
	 */
	sort_name = nemo_canvas_view_get_directory_sort_by (canvas_view, file);
	set_sort_criterion (canvas_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	/* Set the sort direction from the metadata. */
	set_sort_reversed (canvas_view, nemo_canvas_view_get_directory_sort_reversed (canvas_view, file), FALSE);

	nemo_canvas_container_set_keep_aligned (get_canvas_container (canvas_view),
                    nemo_canvas_view_get_directory_keep_aligned (canvas_view, file));

	nemo_canvas_container_set_tighter_layout (get_canvas_container (canvas_view),
                    nemo_canvas_view_get_directory_tighter_layout (canvas_view, file));

	set_labels_beside_icons (canvas_view);
	set_columns_same_width (canvas_view);

	/* We must set auto-layout last, because it invokes the layout_changed 
	 * callback, which works incorrectly if the other layout criteria are
	 * not already set up properly (see bug 6500, e.g.)
	 */
	nemo_canvas_container_set_auto_layout
		(get_canvas_container (canvas_view), 
		 nemo_canvas_view_get_directory_auto_layout (canvas_view, file));

	/* e.g. keep aligned may have changed */
	update_layout_menus (canvas_view);
}

static void
canvas_view_notify_clipboard_info (NemoClipboardMonitor *monitor,
                                 NemoClipboardInfo *info,
                                 NemoCanvasView *canvas_view)
{
	GList *icon_data;

	icon_data = NULL;
	if (info && info->cut) {
		icon_data = info->files;
	}

	nemo_canvas_container_set_highlighted_for_clipboard (
							       get_canvas_container (canvas_view), icon_data);
}

static void
nemo_canvas_view_end_loading (NemoView *view,
			  gboolean all_files_seen)
{
	NemoCanvasView *canvas_view;
	GtkWidget *canvas_container;
	NemoClipboardMonitor *monitor;
	NemoClipboardInfo *info;

	canvas_view = NEMO_CANVAS_VIEW (view);

	canvas_container = GTK_WIDGET (get_canvas_container (canvas_view));
	nemo_canvas_container_end_loading (NEMO_CANVAS_CONTAINER (canvas_container), all_files_seen);

	monitor = nemo_clipboard_monitor_get ();
	info = nemo_clipboard_monitor_get_clipboard_info (monitor);

	canvas_view_notify_clipboard_info (monitor, info, canvas_view);
}

static NemoZoomLevel
nemo_canvas_view_get_zoom_level (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), NEMO_ZOOM_LEVEL_STANDARD);
	
	return nemo_canvas_container_get_zoom_level (get_canvas_container (NEMO_CANVAS_VIEW (view)));
}

static void
nemo_canvas_view_set_zoom_level (NemoCanvasView *view,
				   NemoZoomLevel new_level,
				   gboolean always_emit)
{
	NemoCanvasContainer *canvas_container;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));
	g_return_if_fail (new_level >= NEMO_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NEMO_ZOOM_LEVEL_LARGEST);

	canvas_container = get_canvas_container (view);
	if (nemo_canvas_container_get_zoom_level (canvas_container) == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (view, "zoom_level_changed");
		}
		return;
	}

    if (nemo_global_preferences_get_ignore_view_metadata ()) {
        nemo_window_set_ignore_meta_zoom_level (nemo_view_get_window (NEMO_VIEW (view)), new_level);
    } else {
    	if (view->details->compact) {
    		nemo_file_set_integer_metadata
    			(nemo_view_get_directory_as_file (NEMO_VIEW (view)), 
    			 NEMO_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL, 
    			 get_default_zoom_level (view),
    			 new_level);
    	} else {
    		nemo_file_set_integer_metadata
    			(nemo_view_get_directory_as_file (NEMO_VIEW (view)), 
    			 NEMO_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
    			 get_default_zoom_level (view),
    			 new_level);
    	}
    }

	nemo_canvas_container_set_zoom_level (canvas_container, new_level);

	g_signal_emit_by_name (view, "zoom_level_changed");
	
	if (nemo_view_get_active (NEMO_VIEW (view))) {
		nemo_view_update_menus (NEMO_VIEW (view));
	}
}

static void
nemo_canvas_view_bump_zoom_level (NemoView *view, int zoom_increment)
{
	NemoZoomLevel new_level;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	new_level = nemo_canvas_view_get_zoom_level (view) + zoom_increment;

	if (new_level >= NEMO_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NEMO_ZOOM_LEVEL_LARGEST) {
		nemo_view_zoom_to_level (view, new_level);
	}
}

static void
nemo_canvas_view_zoom_to_level (NemoView *view,
			    NemoZoomLevel zoom_level)
{
	NemoCanvasView *canvas_view;

	g_assert (NEMO_IS_CANVAS_VIEW (view));

	canvas_view = NEMO_CANVAS_VIEW (view);
	nemo_canvas_view_set_zoom_level (canvas_view, zoom_level, FALSE);
}

static void
nemo_canvas_view_restore_default_zoom_level (NemoView *view)
{
	NemoCanvasView *canvas_view;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	canvas_view = NEMO_CANVAS_VIEW (view);
	nemo_view_zoom_to_level
		(view, get_default_zoom_level (canvas_view));
}

static NemoZoomLevel
nemo_canvas_view_get_default_zoom_level (NemoView *view)
{
    g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), NEMO_ZOOM_LEVEL_NULL);

    return get_default_zoom_level(NEMO_CANVAS_VIEW (view));
}

static gboolean 
nemo_canvas_view_can_zoom_in (NemoView *view) 
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return nemo_canvas_view_get_zoom_level (view) 
		< NEMO_ZOOM_LEVEL_LARGEST;
}

static gboolean 
nemo_canvas_view_can_zoom_out (NemoView *view) 
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return nemo_canvas_view_get_zoom_level (view) 
		> NEMO_ZOOM_LEVEL_SMALLEST;
}

static gboolean
nemo_canvas_view_is_empty (NemoView *view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (view));

	return nemo_canvas_container_is_empty 
		(get_canvas_container (NEMO_CANVAS_VIEW (view)));
}

static GList *
nemo_canvas_view_get_selection (NemoView *view)
{
	GList *list;

	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), NULL);

	list = nemo_canvas_container_get_selection
		(get_canvas_container (NEMO_CANVAS_VIEW (view)));
	nemo_file_list_ref (list);
	return list;
}

static void
count_item (NemoCanvasIconData *icon_data,
	    gpointer callback_data)
{
	guint *count;

	count = callback_data;
	(*count)++;
}

static guint
nemo_canvas_view_get_item_count (NemoView *view)
{
	guint count;

	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), 0);

	count = 0;
	
	nemo_canvas_container_for_each
		(get_canvas_container (NEMO_CANVAS_VIEW (view)),
		 count_item, &count);

	return count;
}

static void
set_sort_criterion_by_sort_type (NemoCanvasView *canvas_view,
				 NemoFileSortType  sort_type)
{
	const SortCriterion *sort;

	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));

	sort = get_sort_criterion_by_sort_type (sort_type);
	g_return_if_fail (sort != NULL);
	
	if (sort == canvas_view->details->sort
	    && nemo_canvas_view_using_auto_layout (canvas_view)) {
		return;
	}

	set_sort_criterion (canvas_view, sort, TRUE);
	nemo_canvas_container_sort (get_canvas_container (canvas_view));
	nemo_canvas_view_reveal_selection (NEMO_VIEW (canvas_view));
}


static void
action_reversed_order_callback (GtkAction *action,
				gpointer user_data)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (user_data);

	if (set_sort_reversed (canvas_view,
			       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
			       TRUE)) {
		nemo_canvas_container_sort (get_canvas_container (canvas_view));
		nemo_canvas_view_reveal_selection (NEMO_VIEW (canvas_view));
	}
}

static void
action_keep_aligned_callback (GtkAction *action,
			      gpointer user_data)
{
	NemoCanvasView *canvas_view;
	NemoFile *file;
	gboolean keep_aligned;

	canvas_view = NEMO_CANVAS_VIEW (user_data);

	keep_aligned = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));
	nemo_canvas_view_set_directory_keep_aligned (canvas_view,
						 file,
						 keep_aligned);
						      
	nemo_canvas_container_set_keep_aligned (get_canvas_container (canvas_view),
						  keep_aligned);
}

static void
switch_to_manual_layout (NemoCanvasView *canvas_view)
{
	if (!nemo_canvas_view_using_auto_layout (canvas_view)) {
		return;
	}

	canvas_view->details->sort = &sort_criteria[0];
	
	nemo_canvas_container_set_auto_layout
		(get_canvas_container (canvas_view), FALSE);
}

static void
layout_changed_callback (NemoCanvasContainer *container,
			 NemoCanvasView *canvas_view)
{
	NemoFile *file;

	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));

	if (file != NULL) {
		nemo_canvas_view_set_directory_auto_layout
			(canvas_view,
			 file,
			 nemo_canvas_view_using_auto_layout (canvas_view));

		nemo_canvas_view_set_directory_tighter_layout
			(canvas_view,
			file,
			nemo_canvas_view_using_tighter_layout (canvas_view));
	}

	update_layout_menus (canvas_view);
}

static gboolean
nemo_canvas_view_can_rename_file (NemoView *view, NemoFile *file)
{
	if (!(nemo_canvas_view_get_zoom_level (view) > NEMO_ZOOM_LEVEL_SMALLEST)) {
		return FALSE;
	}

	return NEMO_VIEW_CLASS(nemo_canvas_view_parent_class)->can_rename_file (view, file);
}

static void
nemo_canvas_view_start_renaming_file (NemoView *view,
				  NemoFile *file,
				  gboolean select_all)
{
	/* call parent class to make sure the right canvas is selected */
	NEMO_VIEW_CLASS(nemo_canvas_view_parent_class)->start_renaming_file (view, file, select_all);
	
	/* start renaming */
	nemo_canvas_container_start_renaming_selected_item
		(get_canvas_container (NEMO_CANVAS_VIEW (view)), select_all);
}

static const GtkActionEntry canvas_view_entries[] = {
  /* name, stock id, label */  { "Arrange Items", NULL, N_("Arran_ge Items") }, 
  /* name, stock id */         { "Stretch", NULL,
  /* label, accelerator */       N_("Resize Icon..."), NULL,
  /* tooltip */                  N_("Make the selected icons resizable"),
                                 G_CALLBACK (action_stretch_callback) },
  /* name, stock id */         { "Unstretch", NULL,
  /* label, accelerator */       N_("Restore Icons' Original Si_zes"), NULL,
  /* tooltip */                  N_("Restore each selected icons to its original size"),
                                 G_CALLBACK (action_unstretch_callback) },
  /* name, stock id */         { "Clean Up", NULL,
  /* label, accelerator */       N_("_Organize by Name"), NULL,
  /* tooltip */                  N_("Reposition icons to better fit in the window and avoid overlapping"),
                                 G_CALLBACK (action_clean_up_callback) },
};

static const GtkToggleActionEntry canvas_view_toggle_entries[] = {

  /* name, stock id */      { "Tighter Layout", NULL,
  /* label, accelerator */    N_("Compact _Layout"), NULL,
  /* tooltip */               N_("Toggle using a tighter layout scheme"),
                              G_CALLBACK (action_tighter_layout_callback),
                              0 },
  /* name, stock id */      { "Reversed Order", NULL,
  /* label, accelerator */    N_("Re_versed Order"), NULL,
  /* tooltip */               N_("Display icons in the opposite order"),
                              G_CALLBACK (action_reversed_order_callback),
                              0 },
  /* name, stock id */      { "Keep Aligned", NULL,
  /* label, accelerator */    N_("_Keep Aligned"), NULL,
  /* tooltip */               N_("Keep icons lined up on a grid"),
                              G_CALLBACK (action_keep_aligned_callback),
                              0 },
};

static const GtkRadioActionEntry arrange_radio_entries[] = {
  { "Manual Layout", NULL,
    N_("_Manually"), NULL,
    N_("Leave icons wherever they are dropped"),
    NEMO_FILE_SORT_NONE },
  { "Sort by Name", NULL,
    N_("By _Name"), NULL,
    N_("Keep icons sorted by name in rows"),
    NEMO_FILE_SORT_BY_DISPLAY_NAME },
  { "Sort by Size", NULL,
    N_("By _Size"), NULL,
    N_("Keep icons sorted by size in rows"),
    NEMO_FILE_SORT_BY_SIZE },
  { "Sort by Type", NULL,
    N_("By _Type"), NULL,
    N_("Keep icons sorted by type in rows"),
    NEMO_FILE_SORT_BY_TYPE },
  { "Sort by Modification Date", NULL,
    N_("By Modification _Date"), NULL,
    N_("Keep icons sorted by modification date in rows"),
    NEMO_FILE_SORT_BY_MTIME },
  { "Sort by Trash Time", NULL,
    N_("By T_rash Time"), NULL,
    N_("Keep icons sorted by trash time in rows"),
    NEMO_FILE_SORT_BY_TRASHED_TIME },
};

static void
nemo_canvas_view_merge_menus (NemoView *view)
{
	NemoCanvasView *canvas_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	
        g_assert (NEMO_IS_CANVAS_VIEW (view));

	NEMO_VIEW_CLASS (nemo_canvas_view_parent_class)->merge_menus (view);

	canvas_view = NEMO_CANVAS_VIEW (view);

	ui_manager = nemo_view_get_ui_manager (NEMO_VIEW (canvas_view));

	action_group = gtk_action_group_new ("CanvasViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	canvas_view->details->canvas_action_group = action_group;
	gtk_action_group_add_actions (action_group,
				      canvas_view_entries, G_N_ELEMENTS (canvas_view_entries),
				      canvas_view);
	gtk_action_group_add_toggle_actions (action_group, 
					     canvas_view_toggle_entries, G_N_ELEMENTS (canvas_view_toggle_entries),
					     canvas_view);
	gtk_action_group_add_radio_actions (action_group,
					    arrange_radio_entries,
					    G_N_ELEMENTS (arrange_radio_entries),
					    -1,
					    G_CALLBACK (action_sort_radio_callback),
					    canvas_view);
 
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	canvas_view->details->canvas_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nemo/nemo-canvas-view-ui.xml", NULL);

	/* Do one-time state-setting here; context-dependent state-setting
	 * is done in update_menus.
	 */
	if (!nemo_canvas_view_supports_auto_layout (canvas_view)) {
		action = gtk_action_group_get_action (action_group,
						      NEMO_ACTION_ARRANGE_ITEMS);
		gtk_action_set_visible (action, FALSE);
	}

	if (nemo_canvas_view_supports_scaling (canvas_view)) {
		gtk_ui_manager_add_ui (ui_manager,
				       canvas_view->details->canvas_merge_id,
				       POPUP_PATH_CANVAS_APPEARANCE,
				       NEMO_ACTION_STRETCH,
				       NEMO_ACTION_STRETCH,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		gtk_ui_manager_add_ui (ui_manager,
				       canvas_view->details->canvas_merge_id,
				       POPUP_PATH_CANVAS_APPEARANCE,
				       NEMO_ACTION_UNSTRETCH,
				       NEMO_ACTION_UNSTRETCH,
				       GTK_UI_MANAGER_MENUITEM,
				       FALSE);
	}

	update_layout_menus (canvas_view);
}

static void
nemo_canvas_view_unmerge_menus (NemoView *view)
{
	NemoCanvasView *canvas_view;
	GtkUIManager *ui_manager;

	canvas_view = NEMO_CANVAS_VIEW (view);

	NEMO_VIEW_CLASS (nemo_canvas_view_parent_class)->unmerge_menus (view);

	ui_manager = nemo_view_get_ui_manager (view);
	if (ui_manager != NULL) {
		nemo_ui_unmerge_ui (ui_manager,
					&canvas_view->details->canvas_merge_id,
					&canvas_view->details->canvas_action_group);
	}
}

static void
nemo_canvas_view_update_menus (NemoView *view)
{
	NemoCanvasView *canvas_view;
        int selection_count;
	GtkAction *action;
        NemoCanvasContainer *canvas_container;
	gboolean editable;

        canvas_view = NEMO_CANVAS_VIEW (view);

	NEMO_VIEW_CLASS (nemo_canvas_view_parent_class)->update_menus(view);

        selection_count = nemo_view_get_selection_count (view);
        canvas_container = get_canvas_container (canvas_view);

	action = gtk_action_group_get_action (canvas_view->details->canvas_action_group,
					      NEMO_ACTION_STRETCH);
	gtk_action_set_sensitive (action,
				  selection_count == 1
				  && canvas_container != NULL
				  && !nemo_canvas_container_has_stretch_handles (canvas_container));

	gtk_action_set_visible (action,
				nemo_canvas_view_supports_scaling (canvas_view));

	action = gtk_action_group_get_action (canvas_view->details->canvas_action_group,
					      NEMO_ACTION_UNSTRETCH);
	g_object_set (action, "label",
		      (selection_count > 1)
		      ? _("Restore Icons' Original Si_zes")
		      : _("Restore Icon's Original Si_ze"),
		      NULL);
	gtk_action_set_sensitive (action,
				  canvas_container != NULL
				  && nemo_canvas_container_is_stretched (canvas_container));

	gtk_action_set_visible (action,
				nemo_canvas_view_supports_scaling (canvas_view));

	editable = nemo_view_is_editable (view);
	action = gtk_action_group_get_action (canvas_view->details->canvas_action_group,
					      NEMO_ACTION_MANUAL_LAYOUT);
	gtk_action_set_sensitive (action, editable);
}

static void
nemo_canvas_view_reset_to_defaults (NemoView *view)
{
	NemoCanvasContainer *canvas_container;
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (view);
	canvas_container = get_canvas_container (canvas_view);

	clear_sort_criterion (canvas_view);
	nemo_canvas_container_set_keep_aligned 
		(canvas_container, get_default_directory_keep_aligned ());
	nemo_canvas_container_set_tighter_layout
		(canvas_container, get_default_directory_tighter_layout ());

	nemo_canvas_container_sort (canvas_container);

	update_layout_menus (canvas_view);

	nemo_canvas_view_restore_default_zoom_level (view);

    if (nemo_global_preferences_get_ignore_view_metadata ()) {
        NemoWindow *window = nemo_view_get_window (view);
        nemo_window_set_ignore_meta_tighter_layout (window, TIGHTER_NULL);
        nemo_window_set_ignore_meta_zoom_level (window, NEMO_ZOOM_LEVEL_NULL);
    }
}

static void
nemo_canvas_view_select_all (NemoView *view)
{
	NemoCanvasContainer *canvas_container;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NEMO_CANVAS_VIEW (view));
        nemo_canvas_container_select_all (canvas_container);
}

static void
nemo_canvas_view_select_first (NemoView *view)
{
	NemoCanvasContainer *canvas_container;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NEMO_CANVAS_VIEW (view));
        nemo_canvas_container_select_first (canvas_container);
}

static void
nemo_canvas_view_reveal_selection (NemoView *view)
{
	GList *selection;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

        selection = nemo_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		nemo_canvas_container_reveal 
			(get_canvas_container (NEMO_CANVAS_VIEW (view)), 
			 selection->data);
	}

        nemo_file_list_free (selection);
}

static GArray *
nemo_canvas_view_get_selected_icon_locations (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), NULL);

	return nemo_canvas_container_get_selected_icon_locations
		(get_canvas_container (NEMO_CANVAS_VIEW (view)));
}


static void
nemo_canvas_view_set_selection (NemoView *view, GList *selection)
{
	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	nemo_canvas_container_set_selection
		(get_canvas_container (NEMO_CANVAS_VIEW (view)), selection);
}

static void
nemo_canvas_view_invert_selection (NemoView *view)
{
	g_return_if_fail (NEMO_IS_CANVAS_VIEW (view));

	nemo_canvas_container_invert_selection
		(get_canvas_container (NEMO_CANVAS_VIEW (view)));
}

static gboolean
nemo_canvas_view_using_manual_layout (NemoView *view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (view), FALSE);

	return !nemo_canvas_view_using_auto_layout (NEMO_CANVAS_VIEW (view));
}

static void
nemo_canvas_view_widget_to_file_operation_position (NemoView *view,
						GdkPoint *position)
{
	g_assert (NEMO_IS_CANVAS_VIEW (view));

	nemo_canvas_container_widget_to_file_operation_position
		(get_canvas_container (NEMO_CANVAS_VIEW (view)), position);
}

static void
canvas_container_activate_callback (NemoCanvasContainer *container,
				  GList *file_list,
				  NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nemo_view_activate_files (NEMO_VIEW (canvas_view),
				      file_list, 
				      0, TRUE);
}

static void
canvas_container_activate_previewer_callback (NemoCanvasContainer *container,
					    GList *file_list,
					    GArray *locations,
					    NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nemo_view_preview_files (NEMO_VIEW (canvas_view),
				     file_list, locations);
}

/* this is called in one of these cases:
 * - we activate with enter holding shift
 * - we activate with space holding shift
 * - we double click an canvas holding shift
 * - we middle click an canvas
 *
 * If we don't open in new windows by default, the behavior should be
 * - middle click, shift + activate -> open in new tab
 * - shift + double click -> open in new window
 *
 * If we open in new windows by default, the behaviour should be
 * - middle click, or shift + activate, or shift + double-click -> close parent
 */
static void
canvas_container_activate_alternate_callback (NemoCanvasContainer *container,
					    GList *file_list,
					    NemoCanvasView *canvas_view)
{
	GdkEvent *event;
	GdkEventButton *button_event;
	GdkEventKey *key_event;
	gboolean open_in_tab, open_in_window, close_behind;
	NemoWindowOpenFlags flags;

	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	flags = 0;
	event = gtk_get_current_event ();
	open_in_tab = FALSE;
	open_in_window = FALSE;
	close_behind = FALSE;

	if (event->type == GDK_BUTTON_PRESS ||
	    event->type == GDK_BUTTON_RELEASE ||
	    event->type == GDK_2BUTTON_PRESS ||
	    event->type == GDK_3BUTTON_PRESS) {
		button_event = (GdkEventButton *) event;
		open_in_window = ((button_event->state & GDK_SHIFT_MASK) != 0);
		open_in_tab = !open_in_window;
	} else if (event->type == GDK_KEY_PRESS ||
		   event->type == GDK_KEY_RELEASE) {
		key_event = (GdkEventKey *) event;
		open_in_tab = ((key_event->state & GDK_SHIFT_MASK) != 0);
	}

	if (open_in_tab) {
		flags |= NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	if (open_in_window) {
		flags |= NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
	}

	if (close_behind) {
		flags |= NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
	}

	DEBUG ("Activate alternate, open in tab %d, close behind %d, new window %d\n",
	       open_in_tab, close_behind, open_in_window);

	nemo_view_activate_files (NEMO_VIEW (canvas_view), 
				      file_list, 
				      flags,
				      TRUE);
}

static void
band_select_started_callback (NemoCanvasContainer *container,
			      NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nemo_view_start_batching_selection_changes (NEMO_VIEW (canvas_view));
}

static void
band_select_ended_callback (NemoCanvasContainer *container,
			    NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nemo_view_stop_batching_selection_changes (NEMO_VIEW (canvas_view));
}

int
nemo_canvas_view_compare_files (NemoCanvasView   *canvas_view,
				  NemoFile *a,
				  NemoFile *b)
{
	return nemo_file_compare_for_sort
		(a, b, canvas_view->details->sort->sort_type,
		 /* Use type-unsafe cast for performance */
		 nemo_view_should_sort_directories_first ((NemoView *)canvas_view),
		 canvas_view->details->sort_reversed);
}

static int
compare_files (NemoView   *canvas_view,
	       NemoFile *a,
	       NemoFile *b)
{
	return nemo_canvas_view_compare_files ((NemoCanvasView *)canvas_view, a, b);
}


void
nemo_canvas_view_filter_by_screen (NemoCanvasView *canvas_view,
				     gboolean filter)
{
	canvas_view->details->filter_by_screen = filter;
	canvas_view->details->num_screens = gdk_display_get_n_screens (gtk_widget_get_display (GTK_WIDGET (canvas_view)));
}

static void
nemo_canvas_view_screen_changed (GtkWidget *widget,
				   GdkScreen *previous_screen)
{
	NemoView *view;
	GList *files, *l;
	NemoFile *file;
	NemoDirectory *directory;
	NemoCanvasContainer *canvas_container;

	if (GTK_WIDGET_CLASS (nemo_canvas_view_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (nemo_canvas_view_parent_class)->screen_changed (widget, previous_screen);
	}

	view = NEMO_VIEW (widget);
	if (NEMO_CANVAS_VIEW (view)->details->filter_by_screen) {
		canvas_container = get_canvas_container (NEMO_CANVAS_VIEW (view));

		directory = nemo_view_get_model (view);
		files = nemo_directory_get_file_list (directory);

		for (l = files; l != NULL; l = l->next) {
			file = l->data;
			
			if (!should_show_file_on_screen (view, file)) {
				nemo_canvas_view_remove_file (view, file, directory);
			} else {
				if (nemo_canvas_container_add (canvas_container,
								 NEMO_CANVAS_ICON_DATA (file))) {
					nemo_file_ref (file);
				}
			}
		}
		
		nemo_file_list_unref (files);
		g_list_free (files);
	}
}

static gboolean
nemo_canvas_view_scroll_event (GtkWidget *widget,
			   GdkEventScroll *scroll_event)
{
	NemoCanvasView *canvas_view;
	GdkEvent *event_copy;
	GdkEventScroll *scroll_event_copy;
	gboolean ret;

	canvas_view = NEMO_CANVAS_VIEW (widget);

	if (canvas_view->details->compact &&
	    (scroll_event->direction == GDK_SCROLL_UP ||
	     scroll_event->direction == GDK_SCROLL_DOWN ||
	     scroll_event->direction == GDK_SCROLL_SMOOTH)) {
		ret = nemo_view_handle_scroll_event (NEMO_VIEW (canvas_view), scroll_event);
		if (!ret) {
			/* in column-wise layout, re-emit vertical mouse scroll events as horizontal ones,
			 * if they don't bump zoom */
			event_copy = gdk_event_copy ((GdkEvent *) scroll_event);
			scroll_event_copy = (GdkEventScroll *) event_copy;

			/* transform vertical integer smooth scroll events into horizontal events */
			if (scroll_event_copy->direction == GDK_SCROLL_SMOOTH &&
				   scroll_event_copy->delta_x == 0) {
				if (scroll_event_copy->delta_y == 1.0) {
					scroll_event_copy->direction = GDK_SCROLL_DOWN;
				} else if (scroll_event_copy->delta_y == -1.0) {
					scroll_event_copy->direction = GDK_SCROLL_UP;
				}
			}

			if (scroll_event_copy->direction == GDK_SCROLL_UP) {
				scroll_event_copy->direction = GDK_SCROLL_LEFT;
			} else if (scroll_event_copy->direction == GDK_SCROLL_DOWN) {
				scroll_event_copy->direction = GDK_SCROLL_RIGHT;
			}

			ret = GTK_WIDGET_CLASS (nemo_canvas_view_parent_class)->scroll_event (widget, scroll_event_copy);
			gdk_event_free (event_copy);
		}

		return ret;
	}

	return GTK_WIDGET_CLASS (nemo_canvas_view_parent_class)->scroll_event (widget, scroll_event);
}

static void
selection_changed_callback (NemoCanvasContainer *container,
			    NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nemo_view_notify_selection_changed (NEMO_VIEW (canvas_view));
}

static void
canvas_container_context_click_selection_callback (NemoCanvasContainer *container,
						 GdkEventButton *event,
						 NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_CONTAINER (container));
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));

	nemo_view_pop_up_selection_context_menu 
		(NEMO_VIEW (canvas_view), event);
}

static void
canvas_container_context_click_background_callback (NemoCanvasContainer *container,
						  GdkEventButton *event,
						  NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_CONTAINER (container));
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));

	nemo_view_pop_up_background_context_menu 
		(NEMO_VIEW (canvas_view), event);
}

static gboolean
nemo_canvas_view_react_to_canvas_change_idle_callback (gpointer data) 
{        
        NemoCanvasView *canvas_view;
        
        g_assert (NEMO_IS_CANVAS_VIEW (data));
        
        canvas_view = NEMO_CANVAS_VIEW (data);
        canvas_view->details->react_to_canvas_change_idle_id = 0;
        
	/* Rebuild the menus since some of them (e.g. Restore Stretched Icons)
	 * may be different now.
	 */
	nemo_view_update_menus (NEMO_VIEW (canvas_view));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
icon_position_changed_callback (NemoCanvasContainer *container,
				NemoFile *file,
				const NemoCanvasPosition *position,
				NemoCanvasView *canvas_view)
{
	char *position_string;
	char scale_string[G_ASCII_DTOSTR_BUF_SIZE];

	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));
	g_assert (NEMO_IS_FILE (file));

	/* Schedule updating menus for the next idle. Doing it directly here
	 * noticeably slows down canvas stretching.  The other work here to
	 * store the canvas position and scale does not seem to noticeably
	 * slow down canvas stretching. It would be trickier to move to an
	 * idle call, because we'd have to keep track of potentially multiple
	 * sets of file/geometry info.
	 */
	if (nemo_view_get_active (NEMO_VIEW (canvas_view)) &&
	    canvas_view->details->react_to_canvas_change_idle_id == 0) {
                canvas_view->details->react_to_canvas_change_idle_id
                        = g_idle_add (nemo_canvas_view_react_to_canvas_change_idle_callback,
				      canvas_view);
	}

	/* Store the new position of the canvas in the metadata. */
	if (!nemo_canvas_view_using_auto_layout (canvas_view)) {
		position_string = g_strdup_printf
			("%d,%d", position->x, position->y);
		nemo_file_set_metadata
			(file, NEMO_METADATA_KEY_ICON_POSITION, 
			 NULL, position_string);
		g_free (position_string);
	}


	g_ascii_dtostr (scale_string, sizeof (scale_string), position->scale);
	nemo_file_set_metadata
		(file, NEMO_METADATA_KEY_ICON_SCALE,
		 "1.0", scale_string);
}

/* Attempt to change the filename to the new text.  Notify user if operation fails. */
static void
icon_rename_ended_cb (NemoCanvasContainer *container,
		      NemoFile *file,				    
		      const char *new_name,
		      NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_FILE (file));

	nemo_view_set_is_renaming (NEMO_VIEW (canvas_view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original 
	 * without notifying the user.
	 */
	if ((new_name == NULL) || (new_name[0] == '\0')) {
		return;
	}

	nemo_rename_file (file, new_name, NULL, NULL);
}

static void
icon_rename_started_cb (NemoCanvasContainer *container,
			GtkWidget *widget,
			gpointer callback_data)
{
	NemoView *directory_view;

	directory_view = NEMO_VIEW (callback_data);
	nemo_clipboard_set_up_editable
		(GTK_EDITABLE (widget),
		 nemo_view_get_ui_manager (directory_view),
		 FALSE);
}

static char *
get_icon_uri_callback (NemoCanvasContainer *container,
		       NemoFile *file,
		       NemoCanvasView *canvas_view)
{
	g_assert (NEMO_IS_CANVAS_CONTAINER (container));
	g_assert (NEMO_IS_FILE (file));
	g_assert (NEMO_IS_CANVAS_VIEW (canvas_view));

	return nemo_file_get_uri (file);
}

static char *
get_icon_drop_target_uri_callback (NemoCanvasContainer *container,
		       		   NemoFile *file,
		       		   NemoCanvasView *canvas_view)
{
	g_return_val_if_fail (NEMO_IS_CANVAS_CONTAINER (container), NULL);
	g_return_val_if_fail (NEMO_IS_FILE (file), NULL);
	g_return_val_if_fail (NEMO_IS_CANVAS_VIEW (canvas_view), NULL);

	return nemo_file_get_drop_target_uri (file);
}

/* Preferences changed callbacks */
static void
nemo_canvas_view_click_policy_changed (NemoView *directory_view)
{
	g_assert (NEMO_IS_CANVAS_VIEW (directory_view));

	nemo_canvas_view_update_click_mode (NEMO_CANVAS_VIEW (directory_view));
}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	nemo_canvas_container_request_update_all (get_canvas_container (canvas_view));
}

static void
text_attribute_names_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	nemo_canvas_container_request_update_all (get_canvas_container (canvas_view));
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;
	NemoFile *file;
	char *sort_name;
	NemoCanvasContainer *canvas_container;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (callback_data));

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));
	sort_name = nemo_canvas_view_get_directory_sort_by (canvas_view, file);
	set_sort_criterion (canvas_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	canvas_container = get_canvas_container (canvas_view);
	g_return_if_fail (NEMO_IS_CANVAS_CONTAINER (canvas_container));

	nemo_canvas_container_request_update_all (canvas_container);
}

static void
default_sort_in_reverse_order_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;
	NemoFile *file;
	NemoCanvasContainer *canvas_container;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (callback_data));

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));
	set_sort_reversed (canvas_view, nemo_canvas_view_get_directory_sort_reversed (canvas_view, file), FALSE);
	canvas_container = get_canvas_container (canvas_view);
	g_return_if_fail (NEMO_IS_CANVAS_CONTAINER (canvas_container));

	nemo_canvas_container_request_update_all (canvas_container);
}

static void
default_use_tighter_layout_changed_callback (gpointer callback_data)
{
   NemoCanvasView *canvas_view;
   NemoFile *file;
   NemoCanvasContainer *canvas_container;

   g_return_if_fail (NEMO_IS_CANVAS_VIEW (callback_data));

   canvas_view = NEMO_CANVAS_VIEW (callback_data);

   file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));
   canvas_container = get_canvas_container (canvas_view);
   g_return_if_fail (NEMO_IS_CANVAS_CONTAINER (canvas_container));

   nemo_canvas_container_set_tighter_layout (canvas_container,
                           nemo_canvas_view_get_directory_tighter_layout (canvas_view, file));

   nemo_canvas_container_request_update_all (canvas_container);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;
	NemoFile *file;
	int level;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (callback_data));

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	if (nemo_view_supports_zooming (NEMO_VIEW (canvas_view))) {
		file = nemo_view_get_directory_as_file (NEMO_VIEW (canvas_view));

        if (nemo_global_preferences_get_ignore_view_metadata () &&
            nemo_window_get_ignore_meta_zoom_level (nemo_view_get_window (NEMO_VIEW (canvas_view))) > -1) {
            level = nemo_window_get_ignore_meta_zoom_level (nemo_view_get_window (NEMO_VIEW (canvas_view)));
        } else {
    		if (nemo_canvas_view_is_compact (canvas_view)) {
    			level = nemo_file_get_integer_metadata (file, 
    								    NEMO_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL, 
    								    get_default_zoom_level (canvas_view));
    		} else {
    			level = nemo_file_get_integer_metadata (file, 
    								    NEMO_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL, 
    								    get_default_zoom_level (canvas_view));
    		}
        }
		nemo_view_zoom_to_level (NEMO_VIEW (canvas_view), level);
	}
}

static void
labels_beside_icons_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;

	g_return_if_fail (NEMO_IS_CANVAS_VIEW (callback_data));

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	set_labels_beside_icons (canvas_view);
}

static void
all_columns_same_width_changed_callback (gpointer callback_data)
{
	NemoCanvasView *canvas_view;

	g_assert (NEMO_IS_CANVAS_VIEW (callback_data));

	canvas_view = NEMO_CANVAS_VIEW (callback_data);

	set_columns_same_width (canvas_view);
}


static void
nemo_canvas_view_sort_directories_first_changed (NemoView *directory_view)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (directory_view);

	if (nemo_canvas_view_using_auto_layout (canvas_view)) {
		nemo_canvas_container_sort 
			(get_canvas_container (canvas_view));
	}
}

static gboolean
canvas_view_can_accept_item (NemoCanvasContainer *container,
			   NemoFile *target_item,
			   const char *item_uri,
			   NemoView *view)
{
	return nemo_drag_can_accept_item (target_item, item_uri);
}

static char *
canvas_view_get_container_uri (NemoCanvasContainer *container,
			     NemoView *view)
{
	return nemo_view_get_uri (view);
}

static void
canvas_view_move_copy_items (NemoCanvasContainer *container,
			   const GList *item_uris,
			   GArray *relative_item_points,
			   const char *target_dir,
			   int copy_action,
			   int x, int y,
			   NemoView *view)
{
	nemo_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    nemo_view_get_copied_files_atom (view));
	nemo_view_move_copy_items (view, item_uris, relative_item_points, target_dir,
				       copy_action, x, y);
}

static void
nemo_canvas_view_update_click_mode (NemoCanvasView *canvas_view)
{
	NemoCanvasContainer	*canvas_container;
	int			click_mode;

	canvas_container = get_canvas_container (canvas_view);
	g_assert (canvas_container != NULL);

	click_mode = g_settings_get_enum (nemo_preferences, NEMO_PREFERENCES_CLICK_POLICY);

	nemo_canvas_container_set_single_click_mode (canvas_container,
						       click_mode == NEMO_CLICK_POLICY_SINGLE);
}

static gboolean
get_stored_layout_timestamp (NemoCanvasContainer *container,
			     NemoCanvasIconData *icon_data,
			     time_t *timestamp,
			     NemoCanvasView *view)
{
	NemoFile *file;
	NemoDirectory *directory;

	if (icon_data == NULL) {
		directory = nemo_view_get_model (NEMO_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nemo_directory_get_corresponding_file (directory);
		*timestamp = nemo_file_get_time_metadata (file,
							      NEMO_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP);
		nemo_file_unref (file);
	} else {
		*timestamp = nemo_file_get_time_metadata (NEMO_FILE (icon_data),
							      NEMO_METADATA_KEY_ICON_POSITION_TIMESTAMP);
	}

	return TRUE;
}

static gboolean
store_layout_timestamp (NemoCanvasContainer *container,
			NemoCanvasIconData *icon_data,
			const time_t *timestamp,
			NemoCanvasView *view)
{
	NemoFile *file;
	NemoDirectory *directory;

	if (icon_data == NULL) {
		directory = nemo_view_get_model (NEMO_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nemo_directory_get_corresponding_file (directory);
		nemo_file_set_time_metadata (file,
						 NEMO_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP,
						 (time_t) *timestamp);
		nemo_file_unref (file);
	} else {
		nemo_file_set_time_metadata (NEMO_FILE (icon_data),
						 NEMO_METADATA_KEY_ICON_POSITION_TIMESTAMP,
						 (time_t) *timestamp);
	}

	return TRUE;
}

static gboolean
focus_in_event_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	NemoWindowSlot *slot;
	NemoCanvasView *canvas_view = NEMO_CANVAS_VIEW (user_data);
	
	/* make the corresponding slot (and the window that contains it) active */
	slot = nemo_view_get_nemo_window_slot (NEMO_VIEW (canvas_view));
	nemo_window_slot_make_hosting_pane_active (slot);

	return FALSE; 
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    NemoCanvasView *view = NEMO_CANVAS_VIEW (user_data);

    if (!nemo_view_get_active (NEMO_VIEW (view))) {
        NemoWindowSlot *slot = nemo_view_get_nemo_window_slot (NEMO_VIEW (view));
        nemo_window_slot_make_hosting_pane_active (slot);
        return TRUE;
    }

    return FALSE;
}

static NemoCanvasContainer *
create_canvas_container (NemoCanvasView *canvas_view)
{
	NemoCanvasContainer *canvas_container;

	canvas_container = nemo_canvas_view_container_new (canvas_view);
	canvas_view->details->canvas_container = GTK_WIDGET (canvas_container);
	g_object_add_weak_pointer (G_OBJECT (canvas_container),
				   (gpointer *) &canvas_view->details->canvas_container);
	
	gtk_widget_set_can_focus (GTK_WIDGET (canvas_container), TRUE);
	

    g_signal_connect_object (canvas_container, "button_press_event",
                 G_CALLBACK (button_press_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "focus_in_event",
				 G_CALLBACK (focus_in_event_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "activate",	
				 G_CALLBACK (canvas_container_activate_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "activate_alternate",	
				 G_CALLBACK (canvas_container_activate_alternate_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "activate_previewer",
				 G_CALLBACK (canvas_container_activate_previewer_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "band_select_started",
				 G_CALLBACK (band_select_started_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "band_select_ended",
				 G_CALLBACK (band_select_ended_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "context_click_selection",
				 G_CALLBACK (canvas_container_context_click_selection_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "context_click_background",
				 G_CALLBACK (canvas_container_context_click_background_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon_position_changed",
				 G_CALLBACK (icon_position_changed_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "selection_changed",
				 G_CALLBACK (selection_changed_callback), canvas_view, 0);
	/* FIXME: many of these should move into fm-canvas-container as virtual methods */
	g_signal_connect_object (canvas_container, "get_icon_uri",
				 G_CALLBACK (get_icon_uri_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get_icon_drop_target_uri",
				 G_CALLBACK (get_icon_drop_target_uri_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "move_copy_items",
				 G_CALLBACK (canvas_view_move_copy_items), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get_container_uri",
				 G_CALLBACK (canvas_view_get_container_uri), canvas_view, 0);
	g_signal_connect_object (canvas_container, "can_accept_item",
				 G_CALLBACK (canvas_view_can_accept_item), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get_stored_icon_position",
				 G_CALLBACK (get_stored_icon_position_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "layout_changed",
				 G_CALLBACK (layout_changed_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon_rename_started",
				 G_CALLBACK (icon_rename_started_cb), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon_rename_ended",
				 G_CALLBACK (icon_rename_ended_cb), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon_stretch_started",
				 G_CALLBACK (nemo_view_update_menus), canvas_view,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (canvas_container, "icon_stretch_ended",
				 G_CALLBACK (nemo_view_update_menus), canvas_view,
				 G_CONNECT_SWAPPED);

	g_signal_connect_object (canvas_container, "get_stored_layout_timestamp",
				 G_CALLBACK (get_stored_layout_timestamp), canvas_view, 0);
	g_signal_connect_object (canvas_container, "store_layout_timestamp",
				 G_CALLBACK (store_layout_timestamp), canvas_view, 0);

	gtk_container_add (GTK_CONTAINER (canvas_view),
			   GTK_WIDGET (canvas_container));

	nemo_canvas_view_update_click_mode (canvas_view);

	gtk_widget_show (GTK_WIDGET (canvas_container));

	return canvas_container;
}

/* Handles an URL received from Mozilla */
static void
canvas_view_handle_netscape_url (NemoCanvasContainer *container, const char *encoded_url,
			       const char *target_uri,
			       GdkDragAction action, int x, int y, NemoCanvasView *view)
{
	nemo_view_handle_netscape_url_drop (NEMO_VIEW (view),
						encoded_url, target_uri, action, x, y);
}

static void
canvas_view_handle_uri_list (NemoCanvasContainer *container, const char *item_uris,
			   const char *target_uri,
			   GdkDragAction action, int x, int y, NemoCanvasView *view)
{
	nemo_view_handle_uri_list_drop (NEMO_VIEW (view),
					    item_uris, target_uri, action, x, y);
}

static void
canvas_view_handle_text (NemoCanvasContainer *container, const char *text,
		       const char *target_uri,
		       GdkDragAction action, int x, int y, NemoCanvasView *view)
{
	nemo_view_handle_text_drop (NEMO_VIEW (view),
					text, target_uri, action, x, y);
}

static void
canvas_view_handle_raw (NemoCanvasContainer *container, const char *raw_data,
		      int length, const char *target_uri, const char *direct_save_uri,
		      GdkDragAction action, int x, int y, NemoCanvasView *view)
{
	nemo_view_handle_raw_drop (NEMO_VIEW (view),
				       raw_data, length, target_uri, direct_save_uri, action, x, y);
}

static char *
canvas_view_get_first_visible_file (NemoView *view)
{
	NemoFile *file;
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (view);

	file = NEMO_FILE (nemo_canvas_container_get_first_visible_icon (get_canvas_container (canvas_view)));

	if (file) {
		return nemo_file_get_uri (file);
	}
	
	return NULL;
}

static void
canvas_view_scroll_to_file (NemoView *view,
			  const char *uri)
{
	NemoFile *file;
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (view);
	
	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nemo_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nemo_canvas_container_scroll_to_canvas (get_canvas_container (canvas_view),
								NEMO_CANVAS_ICON_DATA (file));
			nemo_file_unref (file);
		}
	}
}

static const char *
nemo_canvas_view_get_id (NemoView *view)
{
	if (nemo_canvas_view_is_compact (NEMO_CANVAS_VIEW (view))) {
		return NEMO_COMPACT_VIEW_ID;
	}

	return NEMO_CANVAS_VIEW_ID;
}

static void
nemo_canvas_view_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
	NemoCanvasView *canvas_view;
  
	canvas_view = NEMO_CANVAS_VIEW (object);

	switch (prop_id)  {
	case PROP_COMPACT:
		canvas_view->details->compact = g_value_get_boolean (value);
		if (canvas_view->details->compact) {
			nemo_canvas_container_set_layout_mode (get_canvas_container (canvas_view),
								 gtk_widget_get_direction (GTK_WIDGET(canvas_view)) == GTK_TEXT_DIR_RTL ?
								 NEMO_CANVAS_LAYOUT_T_B_R_L :
								 NEMO_CANVAS_LAYOUT_T_B_L_R);
			nemo_canvas_container_set_forced_icon_size (get_canvas_container (canvas_view),
								      NEMO_ICON_SIZE_SMALLEST);
		}
		break;
	case PROP_SUPPORTS_AUTO_LAYOUT:
		canvas_view->details->supports_auto_layout = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_MANUAL_LAYOUT:
		canvas_view->details->supports_manual_layout = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_SCALING:
		canvas_view->details->supports_scaling = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_KEEP_ALIGNED:
		canvas_view->details->supports_keep_aligned = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_LABELS_BESIDE_ICONS:
		canvas_view->details->supports_labels_beside_icons = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nemo_canvas_view_finalize (GObject *object)
{
	NemoCanvasView *canvas_view;

	canvas_view = NEMO_CANVAS_VIEW (object);

	g_free (canvas_view->details);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      default_sort_order_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      default_sort_in_reverse_order_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      image_display_policy_changed_callback,
					      canvas_view);
    
	g_signal_handlers_disconnect_by_func (nemo_canvas_view_preferences,
                          default_use_tighter_layout_changed_callback,
                          canvas_view);

	g_signal_handlers_disconnect_by_func (nemo_canvas_view_preferences,
					      default_zoom_level_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_canvas_view_preferences,
					      labels_beside_icons_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_canvas_view_preferences,
					      text_attribute_names_changed_callback,
					      canvas_view);

	g_signal_handlers_disconnect_by_func (nemo_compact_view_preferences,
					      default_zoom_level_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nemo_compact_view_preferences,
					      all_columns_same_width_changed_callback,
					      canvas_view);

	G_OBJECT_CLASS (nemo_canvas_view_parent_class)->finalize (object);
}

static void
nemo_canvas_view_class_init (NemoCanvasViewClass *klass)
{
	NemoViewClass *nemo_view_class;
	GObjectClass *oclass;

	nemo_view_class = NEMO_VIEW_CLASS (klass);
	oclass = G_OBJECT_CLASS (klass);

	oclass->set_property = nemo_canvas_view_set_property;
	oclass->finalize = nemo_canvas_view_finalize;

	GTK_WIDGET_CLASS (klass)->destroy = nemo_canvas_view_destroy;
	GTK_WIDGET_CLASS (klass)->screen_changed = nemo_canvas_view_screen_changed;
	GTK_WIDGET_CLASS (klass)->scroll_event = nemo_canvas_view_scroll_event;
	
	nemo_view_class->add_file = nemo_canvas_view_add_file;
	nemo_view_class->begin_loading = nemo_canvas_view_begin_loading;
	nemo_view_class->bump_zoom_level = nemo_canvas_view_bump_zoom_level;
	nemo_view_class->can_rename_file = nemo_canvas_view_can_rename_file;
	nemo_view_class->can_zoom_in = nemo_canvas_view_can_zoom_in;
	nemo_view_class->can_zoom_out = nemo_canvas_view_can_zoom_out;
	nemo_view_class->clear = nemo_canvas_view_clear;
	nemo_view_class->end_loading = nemo_canvas_view_end_loading;
	nemo_view_class->file_changed = nemo_canvas_view_file_changed;
	nemo_view_class->get_selected_icon_locations = nemo_canvas_view_get_selected_icon_locations;
	nemo_view_class->get_selection = nemo_canvas_view_get_selection;
	nemo_view_class->get_selection_for_file_transfer = nemo_canvas_view_get_selection;
	nemo_view_class->get_item_count = nemo_canvas_view_get_item_count;
	nemo_view_class->is_empty = nemo_canvas_view_is_empty;
	nemo_view_class->remove_file = nemo_canvas_view_remove_file;
	nemo_view_class->reset_to_defaults = nemo_canvas_view_reset_to_defaults;
	nemo_view_class->restore_default_zoom_level = nemo_canvas_view_restore_default_zoom_level;
    nemo_view_class->get_default_zoom_level = nemo_canvas_view_get_default_zoom_level;
	nemo_view_class->reveal_selection = nemo_canvas_view_reveal_selection;
	nemo_view_class->select_all = nemo_canvas_view_select_all;
        nemo_view_class->select_first = nemo_canvas_view_select_first;
	nemo_view_class->set_selection = nemo_canvas_view_set_selection;
	nemo_view_class->invert_selection = nemo_canvas_view_invert_selection;
	nemo_view_class->compare_files = compare_files;
	nemo_view_class->zoom_to_level = nemo_canvas_view_zoom_to_level;
	nemo_view_class->get_zoom_level = nemo_canvas_view_get_zoom_level;
        nemo_view_class->click_policy_changed = nemo_canvas_view_click_policy_changed;
        nemo_view_class->merge_menus = nemo_canvas_view_merge_menus;
        nemo_view_class->unmerge_menus = nemo_canvas_view_unmerge_menus;
        nemo_view_class->sort_directories_first_changed = nemo_canvas_view_sort_directories_first_changed;
        nemo_view_class->start_renaming_file = nemo_canvas_view_start_renaming_file;
        nemo_view_class->update_menus = nemo_canvas_view_update_menus;
	nemo_view_class->using_manual_layout = nemo_canvas_view_using_manual_layout;
	nemo_view_class->widget_to_file_operation_position = nemo_canvas_view_widget_to_file_operation_position;
	nemo_view_class->get_view_id = nemo_canvas_view_get_id;
	nemo_view_class->get_first_visible_file = canvas_view_get_first_visible_file;
	nemo_view_class->scroll_to_file = canvas_view_scroll_to_file;

	properties[PROP_COMPACT] =
		g_param_spec_boolean ("compact",
				      "Compact",
				      "Whether this view provides a compact listing",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_AUTO_LAYOUT] =
		g_param_spec_boolean ("supports-auto-layout",
				      "Supports auto layout",
				      "Whether this view supports auto layout",
				      TRUE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_MANUAL_LAYOUT] =
		g_param_spec_boolean ("supports-manual-layout",
				      "Supports manual layout",
				      "Whether this view supports manual layout",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_SCALING] =
		g_param_spec_boolean ("supports-scaling",
				      "Supports scaling",
				      "Whether this view supports scaling",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_KEEP_ALIGNED] =
		g_param_spec_boolean ("supports-keep-aligned",
				      "Supports keep aligned",
				      "Whether this view supports keep aligned",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_LABELS_BESIDE_ICONS] =
		g_param_spec_boolean ("supports-labels-beside-icons",
				      "Supports labels beside icons",
				      "Whether this view supports labels beside icons",
				      TRUE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nemo_canvas_view_init (NemoCanvasView *canvas_view)
{
	NemoCanvasContainer *canvas_container;

        g_return_if_fail (gtk_bin_get_child (GTK_BIN (canvas_view)) == NULL);

	canvas_view->details = g_new0 (NemoCanvasViewDetails, 1);
	canvas_view->details->sort = &sort_criteria[0];
	canvas_view->details->filter_by_screen = FALSE;

	canvas_container = create_canvas_container (canvas_view);

	/* Set our default layout mode */
	nemo_canvas_container_set_layout_mode (canvas_container,
						 gtk_widget_get_direction (GTK_WIDGET(canvas_container)) == GTK_TEXT_DIR_RTL ?
						 NEMO_CANVAS_LAYOUT_R_L_T_B :
						 NEMO_CANVAS_LAYOUT_L_R_T_B);

	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_DEFAULT_SORT_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
				  G_CALLBACK (default_sort_in_reverse_order_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_preferences,
				  "changed::" NEMO_PREFERENCES_SHOW_FILE_THUMBNAILS,
				  G_CALLBACK (image_display_policy_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
				  G_CALLBACK (default_use_tighter_layout_changed_callback),
				  canvas_view);

	g_signal_connect_swapped (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_LABELS_BESIDE_ICONS,
				  G_CALLBACK (labels_beside_icons_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_canvas_view_preferences,
				  "changed::" NEMO_PREFERENCES_CANVAS_VIEW_CAPTIONS,
				  G_CALLBACK (text_attribute_names_changed_callback),
				  canvas_view);

	g_signal_connect_swapped (nemo_compact_view_preferences,
				  "changed::" NEMO_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nemo_compact_view_preferences,
				  "changed::" NEMO_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH,
				  G_CALLBACK (all_columns_same_width_changed_callback),
				  canvas_view);

	g_signal_connect_object (get_canvas_container (canvas_view), "handle_netscape_url",
				 G_CALLBACK (canvas_view_handle_netscape_url), canvas_view, 0);
	g_signal_connect_object (get_canvas_container (canvas_view), "handle_uri_list",
				 G_CALLBACK (canvas_view_handle_uri_list), canvas_view, 0);
	g_signal_connect_object (get_canvas_container (canvas_view), "handle_text",
				 G_CALLBACK (canvas_view_handle_text), canvas_view, 0);
	g_signal_connect_object (get_canvas_container (canvas_view), "handle_raw",
				 G_CALLBACK (canvas_view_handle_raw), canvas_view, 0);

	canvas_view->details->clipboard_handler_id =
		g_signal_connect (nemo_clipboard_monitor_get (),
		                  "clipboard_info",
		                  G_CALLBACK (canvas_view_notify_clipboard_info), canvas_view);
}

static NemoView *
nemo_canvas_view_create (NemoWindowSlot *slot)
{
	NemoCanvasView *view;

	view = g_object_new (NEMO_TYPE_CANVAS_VIEW,
			     "window-slot", slot,
			     "compact", FALSE,
			     NULL);
	return NEMO_VIEW (view);
}

static NemoView *
nemo_compact_view_create (NemoWindowSlot *slot)
{
	NemoCanvasView *view;

	view = g_object_new (NEMO_TYPE_CANVAS_VIEW,
			     "window-slot", slot,
			     "compact", TRUE,
			     NULL);
	return NEMO_VIEW (view);
}

static gboolean
nemo_canvas_view_supports_uri (const char *uri,
			   GFileType file_type,
			   const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NEMO_SAVED_SEARCH_MIMETYPE) == 0){
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

#define TRANSLATE_VIEW_INFO(view_info)					\
	view_info.view_combo_label = _(view_info.view_combo_label);	\
	view_info.view_menu_label_with_mnemonic = _(view_info.view_menu_label_with_mnemonic); \
	view_info.error_label = _(view_info.error_label);		\
	view_info.startup_error_label = _(view_info.startup_error_label); \
	view_info.display_location_label = _(view_info.display_location_label); \
	

static NemoViewInfo nemo_canvas_view = {
	NEMO_CANVAS_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	N_("Icon View"),
	/* translators: this is used in the view menu */
	N_("_Icons"),
	N_("The icon view encountered an error."),
	N_("The icon view encountered an error while starting up."),
	N_("Display this location with the icon view."),
	nemo_canvas_view_create,
	nemo_canvas_view_supports_uri
};

static NemoViewInfo nemo_compact_view = {
	NEMO_COMPACT_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	N_("Compact View"),
	/* translators: this is used in the view menu */
	N_("_Compact"),
	N_("The compact view encountered an error."),
	N_("The compact view encountered an error while starting up."),
	N_("Display this location with the compact view."),
	nemo_compact_view_create,
	nemo_canvas_view_supports_uri
};

gboolean
nemo_canvas_view_is_compact (NemoCanvasView *view)
{
	return view->details->compact;
}

void
nemo_canvas_view_register (void)
{
	TRANSLATE_VIEW_INFO (nemo_canvas_view)
		nemo_view_factory_register (&nemo_canvas_view);
}

void
nemo_canvas_view_compact_register (void)
{
	TRANSLATE_VIEW_INFO (nemo_compact_view)
		nemo_view_factory_register (&nemo_compact_view);
}

