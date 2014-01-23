/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-canvas-view.c - implementation of canvas view of directory.

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
   see <http://www.gnu.org/licenses/>.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nautilus-canvas-view.h"

#include "nautilus-actions.h"
#include "nautilus-canvas-view-container.h"
#include "nautilus-desktop-canvas-view.h"
#include "nautilus-error-reporting.h"
#include "nautilus-view-dnd.h"

#include <stdlib.h>
#include <eel/eel-vfs-extensions.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-clipboard-monitor.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-dnd.h>
#include <libnautilus-private/nautilus-file-dnd.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-ui-utilities.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <libnautilus-private/nautilus-canvas-container.h>
#include <libnautilus-private/nautilus-canvas-dnd.h>
#include <libnautilus-private/nautilus-link.h>
#include <libnautilus-private/nautilus-metadata.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-desktop-icon-file.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_CANVAS_VIEW
#include <libnautilus-private/nautilus-debug.h>

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum 
{
	PROP_SUPPORTS_AUTO_LAYOUT = 1,
	PROP_SUPPORTS_SCALING,
	PROP_SUPPORTS_KEEP_ALIGNED,
	PROP_SUPPORTS_MANUAL_LAYOUT,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

typedef struct {
	const NautilusFileSortType sort_type;
	const char *metadata_text;
	const char *action;
} SortCriterion;

typedef enum {
	MENU_ITEM_TYPE_STANDARD,
	MENU_ITEM_TYPE_CHECK,
	MENU_ITEM_TYPE_RADIO,
	MENU_ITEM_TYPE_TREE
} MenuItemType;

struct NautilusCanvasViewDetails
{
	GList *icons_not_positioned;

	guint react_to_canvas_change_idle_id;

	const SortCriterion *sort;
	gboolean sort_reversed;

	GtkActionGroup *canvas_action_group;
	guint canvas_merge_id;

	gulong clipboard_handler_id;

	GtkWidget *canvas_container;

	gboolean supports_auto_layout;
	gboolean supports_manual_layout;
	gboolean supports_scaling;
	gboolean supports_keep_aligned;
};


/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] = {
	{
		NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
		"name",
		"Sort by Name"
	},
	{
		NAUTILUS_FILE_SORT_BY_SIZE,
		"size",
		"Sort by Size"
	},
	{
		NAUTILUS_FILE_SORT_BY_TYPE,
		"type",
		"Sort by Type"
	},
	{
		NAUTILUS_FILE_SORT_BY_MTIME,
		"modification date",
		"Sort by Modification Date"
	},
	{
		NAUTILUS_FILE_SORT_BY_ATIME,
		"access date",
		"Sort by Access Date"
	},
	{
		NAUTILUS_FILE_SORT_BY_TRASHED_TIME,
		"trashed",
		NAUTILUS_ACTION_SORT_TRASH_TIME
	},
	{
		NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE,
		NULL,
		NAUTILUS_ACTION_SORT_SEARCH_RELEVANCE,
	}
};

static void                 nautilus_canvas_view_set_directory_sort_by        (NautilusCanvasView           *canvas_view,
									     NautilusFile         *file,
									     const char           *sort_by);
static void                 nautilus_canvas_view_set_zoom_level               (NautilusCanvasView           *view,
									     NautilusZoomLevel     new_level,
									     gboolean              always_emit);
static void                 nautilus_canvas_view_update_click_mode            (NautilusCanvasView           *canvas_view);
static gboolean             nautilus_canvas_view_supports_scaling	      (NautilusCanvasView           *canvas_view);
static void                 nautilus_canvas_view_reveal_selection       (NautilusView               *view);
static const SortCriterion *get_sort_criterion_by_sort_type           (NautilusFileSortType  sort_type);
static void                 set_sort_criterion_by_sort_type           (NautilusCanvasView           *canvas_view,
								       NautilusFileSortType  sort_type);
static gboolean             set_sort_reversed                         (NautilusCanvasView     *canvas_view,
								       gboolean              new_value,
								       gboolean              set_metadata);
static void                 switch_to_manual_layout                   (NautilusCanvasView     *view);
static void                 update_layout_menus                       (NautilusCanvasView     *view);
static NautilusFileSortType get_default_sort_order                    (NautilusFile         *file,
								       gboolean             *reversed);
static void                 nautilus_canvas_view_clear                  (NautilusView         *view);

G_DEFINE_TYPE (NautilusCanvasView, nautilus_canvas_view, NAUTILUS_TYPE_VIEW);

static void
nautilus_canvas_view_destroy (GtkWidget *object)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (object);

	nautilus_canvas_view_clear (NAUTILUS_VIEW (object));

        if (canvas_view->details->react_to_canvas_change_idle_id != 0) {
                g_source_remove (canvas_view->details->react_to_canvas_change_idle_id);
		canvas_view->details->react_to_canvas_change_idle_id = 0;
        }

	if (canvas_view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nautilus_clipboard_monitor_get (),
					     canvas_view->details->clipboard_handler_id);
		canvas_view->details->clipboard_handler_id = 0;
	}

	if (canvas_view->details->icons_not_positioned) {
		nautilus_file_list_free (canvas_view->details->icons_not_positioned);
		canvas_view->details->icons_not_positioned = NULL;
	}

	GTK_WIDGET_CLASS (nautilus_canvas_view_parent_class)->destroy (object);
}

static NautilusCanvasContainer *
get_canvas_container (NautilusCanvasView *canvas_view)
{
	return NAUTILUS_CANVAS_CONTAINER (canvas_view->details->canvas_container);
}

NautilusCanvasContainer *
nautilus_canvas_view_get_canvas_container (NautilusCanvasView *canvas_view)
{
	return get_canvas_container (canvas_view);
}

static gboolean
nautilus_canvas_view_supports_manual_layout (NautilusCanvasView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_manual_layout;
}

static gboolean
get_stored_icon_position_callback (NautilusCanvasContainer *container,
				   NautilusFile *file,
				   NautilusCanvasPosition *position,
				   NautilusCanvasView *canvas_view)
{
	char *position_string, *scale_string;
	gboolean position_good;
	char c;

	g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (position != NULL);
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	if (!nautilus_canvas_view_supports_manual_layout (canvas_view)) {
		return FALSE;
	}

	/* Get the current position of this canvas from the metadata. */
	position_string = nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_POSITION, "");
	position_good = sscanf
		(position_string, " %d , %d %c",
		 &position->x, &position->y, &c) == 2;
	g_free (position_string);

	/* If it is the desktop directory, maybe the gnome-libs metadata has information about it */

	/* Disable scaling if not on the desktop */
	if (nautilus_canvas_view_supports_scaling (canvas_view)) {
		/* Get the scale of the canvas from the metadata. */
		scale_string = nautilus_file_get_metadata
			(file, NAUTILUS_METADATA_KEY_ICON_SCALE, "1");
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
real_set_sort_criterion (NautilusCanvasView *canvas_view,
                         const SortCriterion *sort,
                         gboolean clear,
			 gboolean set_metadata)
{
	NautilusFile *file;

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view));

	if (clear) {
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY, NULL, NULL);
		nautilus_file_set_metadata (file,
					    NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED, NULL, NULL);
		canvas_view->details->sort =
			get_sort_criterion_by_sort_type	(get_default_sort_order
							 (file, &canvas_view->details->sort_reversed));
	} else if (set_metadata) {
		/* Store the new sort setting. */
		nautilus_canvas_view_set_directory_sort_by (canvas_view,
						    file,
						    sort->metadata_text);
	}

	/* Update the layout menus to match the new sort setting. */
	update_layout_menus (canvas_view);
}

static void
set_sort_criterion (NautilusCanvasView *canvas_view,
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
clear_sort_criterion (NautilusCanvasView *canvas_view)
{
	real_set_sort_criterion (canvas_view, NULL, TRUE, TRUE);
}

void
nautilus_canvas_view_clean_up_by_name (NautilusCanvasView *canvas_view)
{
	NautilusCanvasContainer *canvas_container;
	gboolean saved_sort_reversed;

	canvas_container = get_canvas_container (canvas_view);

	/* Hardwire Clean Up to always be by name, in forward order */
	saved_sort_reversed = canvas_view->details->sort_reversed;
	
	set_sort_reversed (canvas_view, FALSE, FALSE);
	set_sort_criterion (canvas_view, &sort_criteria[0], FALSE);

	nautilus_canvas_container_sort (canvas_container);
	nautilus_canvas_container_freeze_icon_positions (canvas_container);

	set_sort_reversed (canvas_view, saved_sort_reversed, FALSE);
}

static gboolean
nautilus_canvas_view_using_auto_layout (NautilusCanvasView *canvas_view)
{
	return nautilus_canvas_container_is_auto_layout 
		(get_canvas_container (canvas_view));
}

static void
action_sort_radio_callback (GtkAction *action,
			    GtkRadioAction *current,
			    NautilusCanvasView *view)
{
	NautilusFileSortType sort_type;
	
	sort_type = gtk_radio_action_get_current_value (current);
	
	/* Note that id might be a toggle item.
	 * Ignore non-sort ids so that they don't cause sorting.
	 */
	if (sort_type == NAUTILUS_FILE_SORT_NONE) {
		switch_to_manual_layout (view);
	} else {
		set_sort_criterion_by_sort_type (view, sort_type);
	}
}

static void
list_covers (NautilusCanvasIconData *data, gpointer callback_data)
{
	GSList **file_list;

	file_list = callback_data;

	*file_list = g_slist_prepend (*file_list, data);
}

static void
unref_cover (NautilusCanvasIconData *data, gpointer callback_data)
{
	nautilus_file_unref (NAUTILUS_FILE (data));
}

static void
nautilus_canvas_view_clear (NautilusView *view)
{
	NautilusCanvasContainer *canvas_container;
	GSList *file_list;
	
	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NAUTILUS_CANVAS_VIEW (view));
	if (!canvas_container)
		return;

	/* Clear away the existing icons. */
	file_list = NULL;
	nautilus_canvas_container_for_each (canvas_container, list_covers, &file_list);
	nautilus_canvas_container_clear (canvas_container);
	g_slist_foreach (file_list, (GFunc)unref_cover, NULL);
	g_slist_free (file_list);
}

static void
nautilus_canvas_view_remove_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusCanvasView *canvas_view;

	/* This used to assert that 'directory == nautilus_view_get_model (view)', but that
	 * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
	 * It seems that somehow we get a files_changed event sent to the view from a directory
	 * that isn't the model, but the code disables the monitor and signal callback handlers when
	 * changing directories. Maybe we can get some more information when this happens.
	 * Further discussion in bug #368178.
	 */
	if (directory != nautilus_view_get_model (view)) {
		char *file_uri, *dir_uri, *model_uri;
		file_uri = nautilus_file_get_uri (file);
		dir_uri = nautilus_directory_get_uri (directory);
		model_uri = nautilus_directory_get_uri (nautilus_view_get_model (view));
		g_warning ("nautilus_canvas_view_remove_file() - directory not canvas view model, shouldn't happen.\n"
			   "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
			   "If you see this, please add this info to http://bugzilla.gnome.org/show_bug.cgi?id=368178",
			   file, file_uri, directory, dir_uri, nautilus_view_get_model (view), model_uri, nautilus_view_get_loading (view));
		g_free (file_uri);
		g_free (dir_uri);
		g_free (model_uri);
	}
	
	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	if (nautilus_canvas_container_remove (get_canvas_container (canvas_view),
					      NAUTILUS_CANVAS_ICON_DATA (file))) {
		nautilus_file_unref (file);
	}
}

static void
nautilus_canvas_view_add_file (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusCanvasView *canvas_view;
	NautilusCanvasContainer *canvas_container;

	g_assert (directory == nautilus_view_get_model (view));
	
	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	canvas_container = get_canvas_container (canvas_view);

	/* Reset scroll region for the first canvas added when loading a directory. */
	if (nautilus_view_get_loading (view) && nautilus_canvas_container_is_empty (canvas_container)) {
		nautilus_canvas_container_reset_scroll_region (canvas_container);
	}

	if (nautilus_canvas_container_add (canvas_container,
					 NAUTILUS_CANVAS_ICON_DATA (file))) {
		nautilus_file_ref (file);
	}
}

static void
nautilus_canvas_view_file_changed (NautilusView *view, NautilusFile *file, NautilusDirectory *directory)
{
	NautilusCanvasView *canvas_view;

	g_assert (directory == nautilus_view_get_model (view));
	
	g_return_if_fail (view != NULL);
	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	nautilus_canvas_container_request_update
		(get_canvas_container (canvas_view),
		 NAUTILUS_CANVAS_ICON_DATA (file));
}

static gboolean
nautilus_canvas_view_supports_auto_layout (NautilusCanvasView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_auto_layout;
}

static gboolean
nautilus_canvas_view_supports_scaling (NautilusCanvasView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_scaling;
}

static gboolean
nautilus_canvas_view_supports_keep_aligned (NautilusCanvasView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return view->details->supports_keep_aligned;
}

static void
update_layout_menus (NautilusCanvasView *view)
{
	gboolean is_auto_layout;
	GtkAction *action;
	const char *action_name;
	NautilusFile *file;

	if (view->details->canvas_action_group == NULL) {
		return;
	}

	is_auto_layout = nautilus_canvas_view_using_auto_layout (view);
	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (view));

	if (nautilus_canvas_view_supports_auto_layout (view)) {
		/* Mark sort criterion. */
		action_name = is_auto_layout ? view->details->sort->action : NAUTILUS_ACTION_MANUAL_LAYOUT;
		action = gtk_action_group_get_action (view->details->canvas_action_group,
						      action_name);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

		action = gtk_action_group_get_action (view->details->canvas_action_group,
						      NAUTILUS_ACTION_REVERSED_ORDER);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      view->details->sort_reversed);
		gtk_action_set_sensitive (action, is_auto_layout);

		action = gtk_action_group_get_action (view->details->canvas_action_group,
		                                      NAUTILUS_ACTION_SORT_TRASH_TIME);

		if (file != NULL && nautilus_file_is_in_trash (file)) {
			gtk_action_set_visible (action, TRUE);
		} else {
			gtk_action_set_visible (action, FALSE);
		}

		action = gtk_action_group_get_action (view->details->canvas_action_group,
		                                      NAUTILUS_ACTION_SORT_SEARCH_RELEVANCE);

		if (file != NULL && nautilus_file_is_in_search (file)) {
			gtk_action_set_visible (action, TRUE);
		} else {
			gtk_action_set_visible (action, FALSE);
		}
	}

	action = gtk_action_group_get_action (view->details->canvas_action_group,
					      NAUTILUS_ACTION_MANUAL_LAYOUT);
	gtk_action_set_visible (action,
				nautilus_canvas_view_supports_manual_layout (view));

	action = gtk_action_group_get_action (view->details->canvas_action_group,
					      NAUTILUS_ACTION_KEEP_ALIGNED);
	gtk_action_set_visible (action,
				nautilus_canvas_view_supports_keep_aligned (view));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nautilus_canvas_container_is_keep_aligned (get_canvas_container (view)));
	gtk_action_set_sensitive (action, !is_auto_layout);
}


static char *
nautilus_canvas_view_get_directory_sort_by (NautilusCanvasView *canvas_view,
					  NautilusFile *file)
{
	const SortCriterion *default_sort_criterion;

	if (!nautilus_canvas_view_supports_auto_layout (canvas_view)) {
		return g_strdup ("name");
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_val_if_fail (default_sort_criterion != NULL, NULL);

	return nautilus_file_get_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text);
}

static NautilusFileSortType
get_default_sort_order (NautilusFile *file, gboolean *reversed)
{
	NautilusFileSortType retval, default_sort_order;
	gboolean default_sort_in_reverse_order;

	default_sort_order = g_settings_get_enum (nautilus_preferences,
						  NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER);
	default_sort_in_reverse_order = g_settings_get_boolean (nautilus_preferences,
								NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

	retval = nautilus_file_get_default_sort_type (file, reversed);

	if (retval == NAUTILUS_FILE_SORT_NONE) {

		if (reversed != NULL) {
			*reversed = default_sort_in_reverse_order;
		}

		retval = CLAMP (default_sort_order, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
				NAUTILUS_FILE_SORT_BY_ATIME);
	}

	return retval;
}

static void
nautilus_canvas_view_set_directory_sort_by (NautilusCanvasView *canvas_view, 
					  NautilusFile *file, 
					  const char *sort_by)
{
	const SortCriterion *default_sort_criterion;

	if (!nautilus_canvas_view_supports_auto_layout (canvas_view)) {
		return;
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_if_fail (default_sort_criterion != NULL);

	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY,
		 default_sort_criterion->metadata_text,
		 sort_by);
}

static gboolean
nautilus_canvas_view_get_directory_sort_reversed (NautilusCanvasView *canvas_view,
						NautilusFile *file)
{
	gboolean reversed;

	if (!nautilus_canvas_view_supports_auto_layout (canvas_view)) {
		return FALSE;
	}

	get_default_sort_order (file, &reversed);
	return nautilus_file_get_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 reversed);
}

static void
nautilus_canvas_view_set_directory_sort_reversed (NautilusCanvasView *canvas_view,
						NautilusFile *file,
						gboolean sort_reversed)
{
	gboolean reversed;

	if (!nautilus_canvas_view_supports_auto_layout (canvas_view)) {
		return;
	}

	get_default_sort_order (file, &reversed);
	nautilus_file_set_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
		 reversed, sort_reversed);
}

static gboolean
get_default_directory_keep_aligned (void)
{
	return TRUE;
}

static gboolean
nautilus_canvas_view_get_directory_keep_aligned (NautilusCanvasView *canvas_view,
					       NautilusFile *file)
{
	if (!nautilus_canvas_view_supports_keep_aligned (canvas_view)) {
		return FALSE;
	}
	
	return  nautilus_file_get_boolean_metadata
		(file,
		 NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned ());
}

static void
nautilus_canvas_view_set_directory_keep_aligned (NautilusCanvasView *canvas_view,
					       NautilusFile *file,
					       gboolean keep_aligned)
{
	if (!nautilus_canvas_view_supports_keep_aligned (canvas_view)) {
		return;
	}

	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
		 get_default_directory_keep_aligned (),
		 keep_aligned);
}

static gboolean
nautilus_canvas_view_get_directory_auto_layout (NautilusCanvasView *canvas_view,
					      NautilusFile *file)
{
	if (!nautilus_canvas_view_supports_auto_layout (canvas_view)) {
		return FALSE;
	}

	if (!nautilus_canvas_view_supports_manual_layout (canvas_view)) {
		return TRUE;
	}

	return nautilus_file_get_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT, TRUE);
}

static void
nautilus_canvas_view_set_directory_auto_layout (NautilusCanvasView *canvas_view,
					      NautilusFile *file,
					gboolean auto_layout)
{
	if (!nautilus_canvas_view_supports_auto_layout (canvas_view) ||
	    !nautilus_canvas_view_supports_manual_layout (canvas_view)) {
		return;
	}

	nautilus_file_set_boolean_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
		 TRUE,
		 auto_layout);
}

static gboolean
set_sort_reversed (NautilusCanvasView *canvas_view,
		   gboolean new_value,
		   gboolean set_metadata)
{
	if (canvas_view->details->sort_reversed == new_value) {
		return FALSE;
	}
	canvas_view->details->sort_reversed = new_value;

	if (set_metadata) {
		/* Store the new sort setting. */
		nautilus_canvas_view_set_directory_sort_reversed (canvas_view, nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view)), new_value);
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
		if (g_strcmp0 (sort_criteria[i].metadata_text, metadata_text) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_sort_type (NautilusFileSortType sort_type)
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

#define DEFAULT_ZOOM_LEVEL(canvas_view) default_zoom_level

static NautilusZoomLevel
get_default_zoom_level (NautilusCanvasView *canvas_view)
{
	NautilusZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nautilus_icon_view_preferences,
						  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (DEFAULT_ZOOM_LEVEL(canvas_view), NAUTILUS_ZOOM_LEVEL_SMALLEST, NAUTILUS_ZOOM_LEVEL_LARGEST);
}

static void
nautilus_canvas_view_begin_loading (NautilusView *view)
{
	NautilusCanvasView *canvas_view;
	GtkWidget *canvas_container;
	NautilusFile *file;
	char *sort_name, *uri;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	file = nautilus_view_get_directory_as_file (view);
	uri = nautilus_file_get_uri (file);
	canvas_container = GTK_WIDGET (get_canvas_container (canvas_view));

	nautilus_canvas_container_begin_loading (NAUTILUS_CANVAS_CONTAINER (canvas_container));

	nautilus_canvas_container_set_allow_moves (NAUTILUS_CANVAS_CONTAINER (canvas_container),
						 !eel_uri_is_search (uri));

	g_free (uri);

	/* Set the sort mode.
	 * It's OK not to resort the icons because the
	 * container doesn't have any icons at this point.
	 */
	sort_name = nautilus_canvas_view_get_directory_sort_by (canvas_view, file);
	set_sort_criterion (canvas_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	/* Set the sort direction from the metadata. */
	set_sort_reversed (canvas_view, nautilus_canvas_view_get_directory_sort_reversed (canvas_view, file), FALSE);

	nautilus_canvas_container_set_keep_aligned
		(get_canvas_container (canvas_view), 
		 nautilus_canvas_view_get_directory_keep_aligned (canvas_view, file));

	/* We must set auto-layout last, because it invokes the layout_changed 
	 * callback, which works incorrectly if the other layout criteria are
	 * not already set up properly (see bug 6500, e.g.)
	 */
	nautilus_canvas_container_set_auto_layout
		(get_canvas_container (canvas_view), 
		 nautilus_canvas_view_get_directory_auto_layout (canvas_view, file));

	/* e.g. keep aligned may have changed */
	update_layout_menus (canvas_view);
}

static void
canvas_view_notify_clipboard_info (NautilusClipboardMonitor *monitor,
                                 NautilusClipboardInfo *info,
                                 NautilusCanvasView *canvas_view)
{
	GList *icon_data;

	icon_data = NULL;
	if (info && info->cut) {
		icon_data = info->files;
	}

	nautilus_canvas_container_set_highlighted_for_clipboard (
							       get_canvas_container (canvas_view), icon_data);
}

static void
nautilus_canvas_view_end_loading (NautilusView *view,
			  gboolean all_files_seen)
{
	NautilusCanvasView *canvas_view;
	GtkWidget *canvas_container;
	NautilusClipboardMonitor *monitor;
	NautilusClipboardInfo *info;

	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	canvas_container = GTK_WIDGET (get_canvas_container (canvas_view));
	nautilus_canvas_container_end_loading (NAUTILUS_CANVAS_CONTAINER (canvas_container), all_files_seen);

	monitor = nautilus_clipboard_monitor_get ();
	info = nautilus_clipboard_monitor_get_clipboard_info (monitor);

	canvas_view_notify_clipboard_info (monitor, info, canvas_view);
}

static NautilusZoomLevel
nautilus_canvas_view_get_zoom_level (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), NAUTILUS_ZOOM_LEVEL_STANDARD);
	
	return nautilus_canvas_container_get_zoom_level (get_canvas_container (NAUTILUS_CANVAS_VIEW (view)));
}

static void
nautilus_canvas_view_set_zoom_level (NautilusCanvasView *view,
				   NautilusZoomLevel new_level,
				   gboolean always_emit)
{
	NautilusCanvasContainer *canvas_container;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));
	g_return_if_fail (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST);

	canvas_container = get_canvas_container (view);
	if (nautilus_canvas_container_get_zoom_level (canvas_container) == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (view, "zoom-level-changed");
		}
		return;
	}

	nautilus_canvas_container_set_zoom_level (canvas_container, new_level);

	g_signal_emit_by_name (view, "zoom-level-changed");
	
	if (nautilus_view_get_active (NAUTILUS_VIEW (view))) {
		nautilus_view_update_menus (NAUTILUS_VIEW (view));
	}
}

static void
nautilus_canvas_view_bump_zoom_level (NautilusView *view, int zoom_increment)
{
	NautilusZoomLevel new_level;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	new_level = nautilus_canvas_view_get_zoom_level (view) + zoom_increment;

	if (new_level >= NAUTILUS_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NAUTILUS_ZOOM_LEVEL_LARGEST) {
		nautilus_view_zoom_to_level (view, new_level);
	}
}

static void
nautilus_canvas_view_zoom_to_level (NautilusView *view,
			    NautilusZoomLevel zoom_level)
{
	NautilusCanvasView *canvas_view;

	g_assert (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	nautilus_canvas_view_set_zoom_level (canvas_view, zoom_level, FALSE);
}

static void
nautilus_canvas_view_restore_default_zoom_level (NautilusView *view)
{
	NautilusCanvasView *canvas_view;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	nautilus_view_zoom_to_level
		(view, get_default_zoom_level (canvas_view));
}

static gboolean 
nautilus_canvas_view_can_zoom_in (NautilusView *view) 
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return nautilus_canvas_view_get_zoom_level (view) 
		< NAUTILUS_ZOOM_LEVEL_LARGEST;
}

static gboolean 
nautilus_canvas_view_can_zoom_out (NautilusView *view) 
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return nautilus_canvas_view_get_zoom_level (view) 
		> NAUTILUS_ZOOM_LEVEL_SMALLEST;
}

static gboolean
nautilus_canvas_view_is_empty (NautilusView *view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (view));

	return nautilus_canvas_container_is_empty 
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)));
}

static GList *
nautilus_canvas_view_get_selection (NautilusView *view)
{
	GList *list;

	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), NULL);

	list = nautilus_canvas_container_get_selection
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)));
	nautilus_file_list_ref (list);
	return list;
}

static void
set_sort_criterion_by_sort_type (NautilusCanvasView *canvas_view,
				 NautilusFileSortType  sort_type)
{
	const SortCriterion *sort;

	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	sort = get_sort_criterion_by_sort_type (sort_type);
	g_return_if_fail (sort != NULL);
	
	if (sort == canvas_view->details->sort
	    && nautilus_canvas_view_using_auto_layout (canvas_view)) {
		return;
	}

	set_sort_criterion (canvas_view, sort, TRUE);
	nautilus_canvas_container_sort (get_canvas_container (canvas_view));
	nautilus_canvas_view_reveal_selection (NAUTILUS_VIEW (canvas_view));
}


static void
action_reversed_order_callback (GtkAction *action,
				gpointer user_data)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (user_data);

	if (set_sort_reversed (canvas_view,
			       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
			       TRUE)) {
		nautilus_canvas_container_sort (get_canvas_container (canvas_view));
		nautilus_canvas_view_reveal_selection (NAUTILUS_VIEW (canvas_view));
	}
}

static void
action_keep_aligned_callback (GtkAction *action,
			      gpointer user_data)
{
	NautilusCanvasView *canvas_view;
	NautilusFile *file;
	gboolean keep_aligned;

	canvas_view = NAUTILUS_CANVAS_VIEW (user_data);

	keep_aligned = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view));
	nautilus_canvas_view_set_directory_keep_aligned (canvas_view,
						 file,
						 keep_aligned);
						      
	nautilus_canvas_container_set_keep_aligned (get_canvas_container (canvas_view),
						  keep_aligned);
}

static void
switch_to_manual_layout (NautilusCanvasView *canvas_view)
{
	if (!nautilus_canvas_view_using_auto_layout (canvas_view)) {
		return;
	}

	canvas_view->details->sort = &sort_criteria[0];
	
	nautilus_canvas_container_set_auto_layout
		(get_canvas_container (canvas_view), FALSE);
}

static void
layout_changed_callback (NautilusCanvasContainer *container,
			 NautilusCanvasView *canvas_view)
{
	NautilusFile *file;

	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view));

	if (file != NULL) {
		nautilus_canvas_view_set_directory_auto_layout
			(canvas_view,
			 file,
			 nautilus_canvas_view_using_auto_layout (canvas_view));
	}

	update_layout_menus (canvas_view);
}

static gboolean
nautilus_canvas_view_can_rename_file (NautilusView *view, NautilusFile *file)
{
	if (!(nautilus_canvas_view_get_zoom_level (view) > NAUTILUS_ZOOM_LEVEL_SMALLEST)) {
		return FALSE;
	}

	return NAUTILUS_VIEW_CLASS(nautilus_canvas_view_parent_class)->can_rename_file (view, file);
}

static void
nautilus_canvas_view_start_renaming_file (NautilusView *view,
				  NautilusFile *file,
				  gboolean select_all)
{
	/* call parent class to make sure the right canvas is selected */
	NAUTILUS_VIEW_CLASS(nautilus_canvas_view_parent_class)->start_renaming_file (view, file, select_all);
	
	/* start renaming */
	nautilus_canvas_container_start_renaming_selected_item
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)), select_all);
}

static const GtkToggleActionEntry canvas_view_toggle_entries[] = {
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
  { NAUTILUS_ACTION_MANUAL_LAYOUT, NULL,
    N_("_Manually"), NULL,
    N_("Leave icons wherever they are dropped"),
    NAUTILUS_FILE_SORT_NONE },
  { "Sort by Name", NULL,
    N_("By _Name"), NULL,
    N_("Keep icons sorted by name in rows"),
    NAUTILUS_FILE_SORT_BY_DISPLAY_NAME },
  { "Sort by Size", NULL,
    N_("By _Size"), NULL,
    N_("Keep icons sorted by size in rows"),
    NAUTILUS_FILE_SORT_BY_SIZE },
  { "Sort by Type", NULL,
    N_("By _Type"), NULL,
    N_("Keep icons sorted by type in rows"),
    NAUTILUS_FILE_SORT_BY_TYPE },
  { "Sort by Modification Date", NULL,
    N_("By Modification _Date"), NULL,
    N_("Keep icons sorted by modification date in rows"),
    NAUTILUS_FILE_SORT_BY_MTIME },
  { "Sort by Access Date", NULL,
    N_("By _Access Date"), NULL,
    N_("Keep icons sorted by access date in rows"),
    NAUTILUS_FILE_SORT_BY_ATIME },
  { NAUTILUS_ACTION_SORT_TRASH_TIME, NULL,
    N_("By T_rash Time"), NULL,
    N_("Keep icons sorted by trash time in rows"),
    NAUTILUS_FILE_SORT_BY_TRASHED_TIME },
  { NAUTILUS_ACTION_SORT_SEARCH_RELEVANCE, NULL,
    N_("By Search Relevance"), NULL,
    N_("Keep icons sorted by search relevance in rows"),
    NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE },
};

static void
nautilus_canvas_view_merge_menus (NautilusView *view)
{
	NautilusCanvasView *canvas_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	
        g_assert (NAUTILUS_IS_CANVAS_VIEW (view));

	NAUTILUS_VIEW_CLASS (nautilus_canvas_view_parent_class)->merge_menus (view);

	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	ui_manager = nautilus_view_get_ui_manager (NAUTILUS_VIEW (canvas_view));

	action_group = gtk_action_group_new ("CanvasViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	canvas_view->details->canvas_action_group = action_group;
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
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/gnome/nautilus/nautilus-canvas-view-ui.xml", NULL);

	update_layout_menus (canvas_view);
}

static void
nautilus_canvas_view_unmerge_menus (NautilusView *view)
{
	NautilusCanvasView *canvas_view;
	GtkUIManager *ui_manager;

	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_canvas_view_parent_class)->unmerge_menus (view);

	ui_manager = nautilus_view_get_ui_manager (view);
	if (ui_manager != NULL) {
		nautilus_ui_unmerge_ui (ui_manager,
					&canvas_view->details->canvas_merge_id,
					&canvas_view->details->canvas_action_group);
	}
}

static void
nautilus_canvas_view_update_menus (NautilusView *view)
{
	NautilusCanvasView *canvas_view;
	GtkAction *action;
	gboolean editable;

        canvas_view = NAUTILUS_CANVAS_VIEW (view);

	NAUTILUS_VIEW_CLASS (nautilus_canvas_view_parent_class)->update_menus(view);

	editable = nautilus_view_is_editable (view);
	action = gtk_action_group_get_action (canvas_view->details->canvas_action_group,
					      NAUTILUS_ACTION_MANUAL_LAYOUT);
	gtk_action_set_sensitive (action, editable);
}

static void
nautilus_canvas_view_reset_to_defaults (NautilusView *view)
{
	NautilusCanvasContainer *canvas_container;
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	canvas_container = get_canvas_container (canvas_view);

	clear_sort_criterion (canvas_view);
	nautilus_canvas_container_set_keep_aligned 
		(canvas_container, get_default_directory_keep_aligned ());

	nautilus_canvas_container_sort (canvas_container);

	update_layout_menus (canvas_view);

	nautilus_canvas_view_restore_default_zoom_level (view);
}

static void
nautilus_canvas_view_select_all (NautilusView *view)
{
	NautilusCanvasContainer *canvas_container;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NAUTILUS_CANVAS_VIEW (view));
        nautilus_canvas_container_select_all (canvas_container);
}

static void
nautilus_canvas_view_select_first (NautilusView *view)
{
	NautilusCanvasContainer *canvas_container;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	canvas_container = get_canvas_container (NAUTILUS_CANVAS_VIEW (view));
        nautilus_canvas_container_select_first (canvas_container);
}

static void
nautilus_canvas_view_reveal_selection (NautilusView *view)
{
	GList *selection;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

        selection = nautilus_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		nautilus_canvas_container_reveal 
			(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)), 
			 selection->data);
	}

        nautilus_file_list_free (selection);
}

static GArray *
nautilus_canvas_view_get_selected_icon_locations (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), NULL);

	return nautilus_canvas_container_get_selected_icon_locations
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)));
}


static void
nautilus_canvas_view_set_selection (NautilusView *view, GList *selection)
{
	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	nautilus_canvas_container_set_selection
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)), selection);
}

static void
nautilus_canvas_view_invert_selection (NautilusView *view)
{
	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (view));

	nautilus_canvas_container_invert_selection
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)));
}

static gboolean
nautilus_canvas_view_using_manual_layout (NautilusView *view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (view), FALSE);

	return !nautilus_canvas_view_using_auto_layout (NAUTILUS_CANVAS_VIEW (view));
}

static void
nautilus_canvas_view_widget_to_file_operation_position (NautilusView *view,
						GdkPoint *position)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (view));

	nautilus_canvas_container_widget_to_file_operation_position
		(get_canvas_container (NAUTILUS_CANVAS_VIEW (view)), position);
}

static void
canvas_container_activate_callback (NautilusCanvasContainer *container,
				  GList *file_list,
				  NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nautilus_view_activate_files (NAUTILUS_VIEW (canvas_view),
				      file_list, 
				      0, TRUE);
}

static void
canvas_container_activate_previewer_callback (NautilusCanvasContainer *container,
					    GList *file_list,
					    GArray *locations,
					    NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nautilus_view_preview_files (NAUTILUS_VIEW (canvas_view),
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
canvas_container_activate_alternate_callback (NautilusCanvasContainer *container,
					    GList *file_list,
					    NautilusCanvasView *canvas_view)
{
	GdkEvent *event;
	GdkEventButton *button_event;
	GdkEventKey *key_event;
	gboolean open_in_tab, open_in_window, close_behind;
	NautilusWindowOpenFlags flags;

	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
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
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	if (open_in_window) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
	}

	if (close_behind) {
		flags |= NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
	}

	DEBUG ("Activate alternate, open in tab %d, close behind %d, new window %d\n",
	       open_in_tab, close_behind, open_in_window);

	nautilus_view_activate_files (NAUTILUS_VIEW (canvas_view), 
				      file_list, 
				      flags,
				      TRUE);
}

static void
band_select_started_callback (NautilusCanvasContainer *container,
			      NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nautilus_view_start_batching_selection_changes (NAUTILUS_VIEW (canvas_view));
}

static void
band_select_ended_callback (NautilusCanvasContainer *container,
			    NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nautilus_view_stop_batching_selection_changes (NAUTILUS_VIEW (canvas_view));
}

int
nautilus_canvas_view_compare_files (NautilusCanvasView   *canvas_view,
				  NautilusFile *a,
				  NautilusFile *b)
{
	return nautilus_file_compare_for_sort
		(a, b, canvas_view->details->sort->sort_type,
		 /* Use type-unsafe cast for performance */
		 nautilus_view_should_sort_directories_first ((NautilusView *)canvas_view),
		 canvas_view->details->sort_reversed);
}

static int
compare_files (NautilusView   *canvas_view,
	       NautilusFile *a,
	       NautilusFile *b)
{
	return nautilus_canvas_view_compare_files ((NautilusCanvasView *)canvas_view, a, b);
}

static void
selection_changed_callback (NautilusCanvasContainer *container,
			    NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));

	nautilus_view_notify_selection_changed (NAUTILUS_VIEW (canvas_view));
}

static void
canvas_container_context_click_selection_callback (NautilusCanvasContainer *container,
						 GdkEventButton *event,
						 NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	nautilus_view_pop_up_selection_context_menu 
		(NAUTILUS_VIEW (canvas_view), event);
}

static void
canvas_container_context_click_background_callback (NautilusCanvasContainer *container,
						  GdkEventButton *event,
						  NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	nautilus_view_pop_up_background_context_menu 
		(NAUTILUS_VIEW (canvas_view), event);
}

static gboolean
nautilus_canvas_view_react_to_canvas_change_idle_callback (gpointer data) 
{        
        NautilusCanvasView *canvas_view;
        
        g_assert (NAUTILUS_IS_CANVAS_VIEW (data));
        
        canvas_view = NAUTILUS_CANVAS_VIEW (data);
        canvas_view->details->react_to_canvas_change_idle_id = 0;
        
	/* Rebuild the menus since some of them (e.g. Restore Stretched Icons)
	 * may be different now.
	 */
	nautilus_view_update_menus (NAUTILUS_VIEW (canvas_view));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
icon_position_changed_callback (NautilusCanvasContainer *container,
				NautilusFile *file,
				const NautilusCanvasPosition *position,
				NautilusCanvasView *canvas_view)
{
	char *position_string;
	char scale_string[G_ASCII_DTOSTR_BUF_SIZE];

	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));
	g_assert (container == get_canvas_container (canvas_view));
	g_assert (NAUTILUS_IS_FILE (file));

	/* Schedule updating menus for the next idle. Doing it directly here
	 * noticeably slows down canvas stretching.  The other work here to
	 * store the canvas position and scale does not seem to noticeably
	 * slow down canvas stretching. It would be trickier to move to an
	 * idle call, because we'd have to keep track of potentially multiple
	 * sets of file/geometry info.
	 */
	if (nautilus_view_get_active (NAUTILUS_VIEW (canvas_view)) &&
	    canvas_view->details->react_to_canvas_change_idle_id == 0) {
                canvas_view->details->react_to_canvas_change_idle_id
                        = g_idle_add (nautilus_canvas_view_react_to_canvas_change_idle_callback,
				      canvas_view);
	}

	/* Store the new position of the canvas in the metadata. */
	if (!nautilus_canvas_view_using_auto_layout (canvas_view)) {
		position_string = g_strdup_printf
			("%d,%d", position->x, position->y);
		nautilus_file_set_metadata
			(file, NAUTILUS_METADATA_KEY_ICON_POSITION, 
			 NULL, position_string);
		g_free (position_string);
	}


	g_ascii_dtostr (scale_string, sizeof (scale_string), position->scale);
	nautilus_file_set_metadata
		(file, NAUTILUS_METADATA_KEY_ICON_SCALE,
		 "1.0", scale_string);
}

/* Attempt to change the filename to the new text.  Notify user if operation fails. */
static void
icon_rename_ended_cb (NautilusCanvasContainer *container,
		      NautilusFile *file,				    
		      const char *new_name,
		      NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_FILE (file));

	nautilus_view_set_is_renaming (NAUTILUS_VIEW (canvas_view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original 
	 * without notifying the user.
	 */
	if ((new_name == NULL) || (new_name[0] == '\0')) {
		return;
	}

	nautilus_rename_file (file, new_name, NULL, NULL);
}

static void
icon_rename_started_cb (NautilusCanvasContainer *container,
			GtkWidget *widget,
			gpointer callback_data)
{
	NautilusView *directory_view;

	directory_view = NAUTILUS_VIEW (callback_data);
	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (widget),
		 nautilus_view_get_ui_manager (directory_view),
		 FALSE);
}

static char *
get_icon_uri_callback (NautilusCanvasContainer *container,
		       NautilusFile *file,
		       NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	return nautilus_file_get_uri (file);
}

static char *
get_icon_activation_uri_callback (NautilusCanvasContainer *container,
				  NautilusFile *file,
				  NautilusCanvasView *canvas_view)
{
	g_assert (NAUTILUS_IS_CANVAS_CONTAINER (container));
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_CANVAS_VIEW (canvas_view));

	return nautilus_file_get_activation_uri (file);
}

static char *
get_icon_drop_target_uri_callback (NautilusCanvasContainer *container,
		       		   NautilusFile *file,
		       		   NautilusCanvasView *canvas_view)
{
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (container), NULL);
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
	g_return_val_if_fail (NAUTILUS_IS_CANVAS_VIEW (canvas_view), NULL);

	return nautilus_file_get_drop_target_uri (file);
}

/* Preferences changed callbacks */
static void
nautilus_canvas_view_click_policy_changed (NautilusView *directory_view)
{
	g_assert (NAUTILUS_IS_CANVAS_VIEW (directory_view));

	nautilus_canvas_view_update_click_mode (NAUTILUS_CANVAS_VIEW (directory_view));
}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (callback_data);

	nautilus_canvas_container_request_update_all (get_canvas_container (canvas_view));
}

static void
text_attribute_names_changed_callback (gpointer callback_data)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (callback_data);

	nautilus_canvas_container_request_update_all (get_canvas_container (canvas_view));
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	NautilusCanvasView *canvas_view;
	NautilusFile *file;
	char *sort_name;
	NautilusCanvasContainer *canvas_container;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (callback_data));

	canvas_view = NAUTILUS_CANVAS_VIEW (callback_data);

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view));
	sort_name = nautilus_canvas_view_get_directory_sort_by (canvas_view, file);
	set_sort_criterion (canvas_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	canvas_container = get_canvas_container (canvas_view);
	g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (canvas_container));

	nautilus_canvas_container_request_update_all (canvas_container);
}

static void
default_sort_in_reverse_order_changed_callback (gpointer callback_data)
{
	NautilusCanvasView *canvas_view;
	NautilusFile *file;
	NautilusCanvasContainer *canvas_container;

	g_return_if_fail (NAUTILUS_IS_CANVAS_VIEW (callback_data));

	canvas_view = NAUTILUS_CANVAS_VIEW (callback_data);

	file = nautilus_view_get_directory_as_file (NAUTILUS_VIEW (canvas_view));
	set_sort_reversed (canvas_view, nautilus_canvas_view_get_directory_sort_reversed (canvas_view, file), FALSE);
	canvas_container = get_canvas_container (canvas_view);
	g_return_if_fail (NAUTILUS_IS_CANVAS_CONTAINER (canvas_container));

	nautilus_canvas_container_request_update_all (canvas_container);
}

static void
nautilus_canvas_view_sort_directories_first_changed (NautilusView *directory_view)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (directory_view);

	if (nautilus_canvas_view_using_auto_layout (canvas_view)) {
		nautilus_canvas_container_sort 
			(get_canvas_container (canvas_view));
	}
}

static gboolean
canvas_view_can_accept_item (NautilusCanvasContainer *container,
			   NautilusFile *target_item,
			   const char *item_uri,
			   NautilusView *view)
{
	return nautilus_drag_can_accept_item (target_item, item_uri);
}

static char *
canvas_view_get_container_uri (NautilusCanvasContainer *container,
			     NautilusView *view)
{
	return nautilus_view_get_uri (view);
}

static void
canvas_view_move_copy_items (NautilusCanvasContainer *container,
			   const GList *item_uris,
			   GArray *relative_item_points,
			   const char *target_dir,
			   int copy_action,
			   int x, int y,
			   NautilusView *view)
{
	nautilus_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    nautilus_view_get_copied_files_atom (view));
	nautilus_view_move_copy_items (view, item_uris, relative_item_points, target_dir,
				       copy_action, x, y);
}

static void
nautilus_canvas_view_update_click_mode (NautilusCanvasView *canvas_view)
{
	NautilusCanvasContainer	*canvas_container;
	int			click_mode;

	canvas_container = get_canvas_container (canvas_view);
	g_assert (canvas_container != NULL);

	click_mode = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_CLICK_POLICY);

	nautilus_canvas_container_set_single_click_mode (canvas_container,
						       click_mode == NAUTILUS_CLICK_POLICY_SINGLE);
}

static gboolean
get_stored_layout_timestamp (NautilusCanvasContainer *container,
			     NautilusCanvasIconData *icon_data,
			     time_t *timestamp,
			     NautilusCanvasView *view)
{
	NautilusFile *file;
	NautilusDirectory *directory;

	if (icon_data == NULL) {
		directory = nautilus_view_get_model (NAUTILUS_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nautilus_directory_get_corresponding_file (directory);
		*timestamp = nautilus_file_get_time_metadata (file,
							      NAUTILUS_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP);
		nautilus_file_unref (file);
	} else {
		*timestamp = nautilus_file_get_time_metadata (NAUTILUS_FILE (icon_data),
							      NAUTILUS_METADATA_KEY_ICON_POSITION_TIMESTAMP);
	}

	return TRUE;
}

static gboolean
store_layout_timestamp (NautilusCanvasContainer *container,
			NautilusCanvasIconData *icon_data,
			const time_t *timestamp,
			NautilusCanvasView *view)
{
	NautilusFile *file;
	NautilusDirectory *directory;

	if (icon_data == NULL) {
		directory = nautilus_view_get_model (NAUTILUS_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nautilus_directory_get_corresponding_file (directory);
		nautilus_file_set_time_metadata (file,
						 NAUTILUS_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP,
						 (time_t) *timestamp);
		nautilus_file_unref (file);
	} else {
		nautilus_file_set_time_metadata (NAUTILUS_FILE (icon_data),
						 NAUTILUS_METADATA_KEY_ICON_POSITION_TIMESTAMP,
						 (time_t) *timestamp);
	}

	return TRUE;
}

static NautilusCanvasContainer *
create_canvas_container (NautilusCanvasView *canvas_view)
{
	NautilusCanvasContainer *canvas_container;

	canvas_container = nautilus_canvas_view_container_new (canvas_view);
	canvas_view->details->canvas_container = GTK_WIDGET (canvas_container);
	g_object_add_weak_pointer (G_OBJECT (canvas_container),
				   (gpointer *) &canvas_view->details->canvas_container);
	
	gtk_widget_set_can_focus (GTK_WIDGET (canvas_container), TRUE);
	
	g_signal_connect_object (canvas_container, "activate",	
				 G_CALLBACK (canvas_container_activate_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "activate-alternate",	
				 G_CALLBACK (canvas_container_activate_alternate_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "activate-previewer",
				 G_CALLBACK (canvas_container_activate_previewer_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "band-select-started",
				 G_CALLBACK (band_select_started_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "band-select-ended",
				 G_CALLBACK (band_select_ended_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "context-click-selection",
				 G_CALLBACK (canvas_container_context_click_selection_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "context-click-background",
				 G_CALLBACK (canvas_container_context_click_background_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon-position-changed",
				 G_CALLBACK (icon_position_changed_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "selection-changed",
				 G_CALLBACK (selection_changed_callback), canvas_view, 0);
	/* FIXME: many of these should move into fm-canvas-container as virtual methods */
	g_signal_connect_object (canvas_container, "get-icon-uri",
				 G_CALLBACK (get_icon_uri_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get-icon-activation-uri",
				 G_CALLBACK (get_icon_activation_uri_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get-icon-drop-target-uri",
				 G_CALLBACK (get_icon_drop_target_uri_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "move-copy-items",
				 G_CALLBACK (canvas_view_move_copy_items), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get-container-uri",
				 G_CALLBACK (canvas_view_get_container_uri), canvas_view, 0);
	g_signal_connect_object (canvas_container, "can-accept-item",
				 G_CALLBACK (canvas_view_can_accept_item), canvas_view, 0);
	g_signal_connect_object (canvas_container, "get-stored-icon-position",
				 G_CALLBACK (get_stored_icon_position_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "layout-changed",
				 G_CALLBACK (layout_changed_callback), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon-rename-started",
				 G_CALLBACK (icon_rename_started_cb), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon-rename-ended",
				 G_CALLBACK (icon_rename_ended_cb), canvas_view, 0);
	g_signal_connect_object (canvas_container, "icon-stretch-started",
				 G_CALLBACK (nautilus_view_update_menus), canvas_view,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (canvas_container, "icon-stretch-ended",
				 G_CALLBACK (nautilus_view_update_menus), canvas_view,
				 G_CONNECT_SWAPPED);

	g_signal_connect_object (canvas_container, "get-stored-layout-timestamp",
				 G_CALLBACK (get_stored_layout_timestamp), canvas_view, 0);
	g_signal_connect_object (canvas_container, "store-layout-timestamp",
				 G_CALLBACK (store_layout_timestamp), canvas_view, 0);

	gtk_container_add (GTK_CONTAINER (canvas_view),
			   GTK_WIDGET (canvas_container));

	nautilus_canvas_view_update_click_mode (canvas_view);
	nautilus_canvas_container_set_zoom_level (canvas_container,
						  get_default_zoom_level (canvas_view));

	gtk_widget_show (GTK_WIDGET (canvas_container));

	return canvas_container;
}

/* Handles an URL received from Mozilla */
static void
canvas_view_handle_netscape_url (NautilusCanvasContainer *container, const char *encoded_url,
			       const char *target_uri,
			       GdkDragAction action, int x, int y, NautilusCanvasView *view)
{
	nautilus_view_handle_netscape_url_drop (NAUTILUS_VIEW (view),
						encoded_url, target_uri, action, x, y);
}

static void
canvas_view_handle_uri_list (NautilusCanvasContainer *container, const char *item_uris,
			   const char *target_uri,
			   GdkDragAction action, int x, int y, NautilusCanvasView *view)
{
	nautilus_view_handle_uri_list_drop (NAUTILUS_VIEW (view),
					    item_uris, target_uri, action, x, y);
}

static void
canvas_view_handle_text (NautilusCanvasContainer *container, const char *text,
		       const char *target_uri,
		       GdkDragAction action, int x, int y, NautilusCanvasView *view)
{
	nautilus_view_handle_text_drop (NAUTILUS_VIEW (view),
					text, target_uri, action, x, y);
}

static void
canvas_view_handle_raw (NautilusCanvasContainer *container, const char *raw_data,
		      int length, const char *target_uri, const char *direct_save_uri,
		      GdkDragAction action, int x, int y, NautilusCanvasView *view)
{
	nautilus_view_handle_raw_drop (NAUTILUS_VIEW (view),
				       raw_data, length, target_uri, direct_save_uri, action, x, y);
}

static void
canvas_view_handle_hover (NautilusCanvasContainer *container,
			  const char *target_uri,
			  NautilusCanvasView *view)
{
	nautilus_view_handle_hover (NAUTILUS_VIEW (view), target_uri);
}

static char *
canvas_view_get_first_visible_file (NautilusView *view)
{
	NautilusFile *file;
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (view);

	file = NAUTILUS_FILE (nautilus_canvas_container_get_first_visible_icon (get_canvas_container (canvas_view)));

	if (file) {
		return nautilus_file_get_uri (file);
	}
	
	return NULL;
}

static void
canvas_view_scroll_to_file (NautilusView *view,
			  const char *uri)
{
	NautilusFile *file;
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (view);
	
	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nautilus_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nautilus_canvas_container_scroll_to_canvas (get_canvas_container (canvas_view),
								NAUTILUS_CANVAS_ICON_DATA (file));
			nautilus_file_unref (file);
		}
	}
}

static const char *
nautilus_canvas_view_get_id (NautilusView *view)
{
	return NAUTILUS_CANVAS_VIEW_ID;
}

static void
nautilus_canvas_view_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
	NautilusCanvasView *canvas_view;
  
	canvas_view = NAUTILUS_CANVAS_VIEW (object);

	switch (prop_id)  {
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
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nautilus_canvas_view_finalize (GObject *object)
{
	NautilusCanvasView *canvas_view;

	canvas_view = NAUTILUS_CANVAS_VIEW (object);

	g_free (canvas_view->details);

	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      default_sort_order_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      default_sort_in_reverse_order_changed_callback,
					      canvas_view);
	g_signal_handlers_disconnect_by_func (nautilus_preferences,
					      image_display_policy_changed_callback,
					      canvas_view);

	g_signal_handlers_disconnect_by_func (nautilus_icon_view_preferences,
					      text_attribute_names_changed_callback,
					      canvas_view);

	G_OBJECT_CLASS (nautilus_canvas_view_parent_class)->finalize (object);
}

static void
nautilus_canvas_view_class_init (NautilusCanvasViewClass *klass)
{
	NautilusViewClass *nautilus_view_class;
	GObjectClass *oclass;

	nautilus_view_class = NAUTILUS_VIEW_CLASS (klass);
	oclass = G_OBJECT_CLASS (klass);

	oclass->set_property = nautilus_canvas_view_set_property;
	oclass->finalize = nautilus_canvas_view_finalize;

	GTK_WIDGET_CLASS (klass)->destroy = nautilus_canvas_view_destroy;
	
	nautilus_view_class->add_file = nautilus_canvas_view_add_file;
	nautilus_view_class->begin_loading = nautilus_canvas_view_begin_loading;
	nautilus_view_class->bump_zoom_level = nautilus_canvas_view_bump_zoom_level;
	nautilus_view_class->can_rename_file = nautilus_canvas_view_can_rename_file;
	nautilus_view_class->can_zoom_in = nautilus_canvas_view_can_zoom_in;
	nautilus_view_class->can_zoom_out = nautilus_canvas_view_can_zoom_out;
	nautilus_view_class->clear = nautilus_canvas_view_clear;
	nautilus_view_class->end_loading = nautilus_canvas_view_end_loading;
	nautilus_view_class->file_changed = nautilus_canvas_view_file_changed;
	nautilus_view_class->get_selected_icon_locations = nautilus_canvas_view_get_selected_icon_locations;
	nautilus_view_class->get_selection = nautilus_canvas_view_get_selection;
	nautilus_view_class->get_selection_for_file_transfer = nautilus_canvas_view_get_selection;
	nautilus_view_class->is_empty = nautilus_canvas_view_is_empty;
	nautilus_view_class->remove_file = nautilus_canvas_view_remove_file;
	nautilus_view_class->reset_to_defaults = nautilus_canvas_view_reset_to_defaults;
	nautilus_view_class->restore_default_zoom_level = nautilus_canvas_view_restore_default_zoom_level;
	nautilus_view_class->reveal_selection = nautilus_canvas_view_reveal_selection;
	nautilus_view_class->select_all = nautilus_canvas_view_select_all;
	nautilus_view_class->select_first = nautilus_canvas_view_select_first;
	nautilus_view_class->set_selection = nautilus_canvas_view_set_selection;
	nautilus_view_class->invert_selection = nautilus_canvas_view_invert_selection;
	nautilus_view_class->compare_files = compare_files;
	nautilus_view_class->zoom_to_level = nautilus_canvas_view_zoom_to_level;
	nautilus_view_class->get_zoom_level = nautilus_canvas_view_get_zoom_level;
        nautilus_view_class->click_policy_changed = nautilus_canvas_view_click_policy_changed;
        nautilus_view_class->merge_menus = nautilus_canvas_view_merge_menus;
        nautilus_view_class->unmerge_menus = nautilus_canvas_view_unmerge_menus;
        nautilus_view_class->sort_directories_first_changed = nautilus_canvas_view_sort_directories_first_changed;
        nautilus_view_class->start_renaming_file = nautilus_canvas_view_start_renaming_file;
        nautilus_view_class->update_menus = nautilus_canvas_view_update_menus;
	nautilus_view_class->using_manual_layout = nautilus_canvas_view_using_manual_layout;
	nautilus_view_class->widget_to_file_operation_position = nautilus_canvas_view_widget_to_file_operation_position;
	nautilus_view_class->get_view_id = nautilus_canvas_view_get_id;
	nautilus_view_class->get_first_visible_file = canvas_view_get_first_visible_file;
	nautilus_view_class->scroll_to_file = canvas_view_scroll_to_file;

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

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nautilus_canvas_view_init (NautilusCanvasView *canvas_view)
{
	NautilusCanvasContainer *canvas_container;

        g_return_if_fail (gtk_bin_get_child (GTK_BIN (canvas_view)) == NULL);

	canvas_view->details = g_new0 (NautilusCanvasViewDetails, 1);
	canvas_view->details->sort = &sort_criteria[0];

	canvas_container = create_canvas_container (canvas_view);

	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
				  G_CALLBACK (default_sort_in_reverse_order_changed_callback),
				  canvas_view);
	g_signal_connect_swapped (nautilus_preferences,
				  "changed::" NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS,
				  G_CALLBACK (image_display_policy_changed_callback),
				  canvas_view);

	g_signal_connect_swapped (nautilus_icon_view_preferences,
				  "changed::" NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
				  G_CALLBACK (text_attribute_names_changed_callback),
				  canvas_view);

	g_signal_connect_object (canvas_container, "handle-netscape-url",
				 G_CALLBACK (canvas_view_handle_netscape_url), canvas_view, 0);
	g_signal_connect_object (canvas_container, "handle-uri-list",
				 G_CALLBACK (canvas_view_handle_uri_list), canvas_view, 0);
	g_signal_connect_object (canvas_container, "handle-text",
				 G_CALLBACK (canvas_view_handle_text), canvas_view, 0);
	g_signal_connect_object (canvas_container, "handle-raw",
				 G_CALLBACK (canvas_view_handle_raw), canvas_view, 0);
	g_signal_connect_object (canvas_container, "handle-hover",
				 G_CALLBACK (canvas_view_handle_hover), canvas_view, 0);

	canvas_view->details->clipboard_handler_id =
		g_signal_connect (nautilus_clipboard_monitor_get (),
		                  "clipboard-info",
		                  G_CALLBACK (canvas_view_notify_clipboard_info), canvas_view);
}

NautilusView *
nautilus_canvas_view_new (NautilusWindowSlot *slot)
{
	return g_object_new (NAUTILUS_TYPE_CANVAS_VIEW,
			     "window-slot", slot,
			     NULL);
}
