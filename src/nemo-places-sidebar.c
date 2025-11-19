/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Nemo
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Cosimo Cecchi <cosimoc@gnome.org>
 *
 */


#include <config.h>

#include "nemo-tree-sidebar-model.h"
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <math.h>
#include <cairo-gobject.h>
#include <libxapp/xapp-favorites.h>

#include <libnemo-private/nemo-clipboard.h>
#include <libnemo-private/nemo-clipboard-monitor.h>
#include <libnemo-private/nemo-desktop-icon-file.h>
#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-bookmark.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <libnemo-private/nemo-icon-names.h>
#include <libnemo-private/nemo-program-choosing.h>
#include <libnemo-private/nemo-tree-view-drag-dest.h>
#include <libnemo-private/nemo-cell-renderer-disk.h>
#include <libnemo-private/nemo-places-tree-view.h>
#include <libnemo-private/nemo-action-manager.h>
#include <libnemo-private/nemo-action.h>
#include <libnemo-private/nemo-ui-utilities.h>
#include <libnemo-private/nemo-directory.h>

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-bookmark-list.h"
#include "nemo-places-sidebar.h"
#include "nemo-properties-window.h"
#include "nemo-window.h"
#include "nemo-window-slot.h"

#define DEBUG_FLAG NEMO_DEBUG_PLACES
#include <libnemo-private/nemo-debug.h>

#define EXPANDER_PAD_COLUMN_WIDTH 4
#define EJECT_COLUMN_MIN_WIDTH 22
#define EJECT_COLUMN_MAX_WIDTH 60
#define DRAG_EXPAND_CATEGORY_DELAY 500
#define EJECT_PAD_COLUMN_WIDTH 14

static gint EJECT_ICON_SIZE_NOT_HOVERED = 0;
static gint EJECT_ICON_SIZE_HOVERED = GTK_ICON_SIZE_MENU;
static gint menu_icon_pixels = 16;
#define EJECT_ICON_SIZE_REDUCTION 4

#if (!GLIB_CHECK_VERSION(2,50,0))
#define g_drive_is_removable g_drive_is_media_removable
#endif


typedef struct {
	GtkScrolledWindow  parent;
	GtkTreeView        *tree_view;
    GtkTreeViewColumn  *eject_column;
    GtkCellRenderer    *eject_icon_cell_renderer;
    GtkCellRenderer    *editable_renderer;
	GtkCellRenderer    *icon_cell_renderer;
	GtkCellRenderer    *padding_cell_renderer;
	GtkCellRenderer    *heading_cell_renderer;
	char 	           *selection_location;
	gboolean            selecting;
	void	       		   *store;
    GtkTreeModel       *sort_model;
    GtkTreeModel       *store_filter;

	NemoWindow *window;
	NemoBookmarkList *bookmarks;
	GVolumeMonitor *volume_monitor;

    NemoActionManager *action_manager;
    gulong actions_changed_id;
    guint actions_changed_idle_id;

    GtkUIManager *ui_manager;

    GtkActionGroup *bookmark_action_group;
    guint bookmark_action_group_merge_id;
    GtkActionGroup *action_action_group;
    guint action_action_group_merge_id;

    GtkActionGroup *tv_action_group;
    guint tv_action_group_merge_id;

    gboolean actions_need_update;

	gboolean devices_header_added;
	gboolean bookmarks_header_added;

	/* DnD */
	GList     *drag_list;
	gboolean  drag_data_received;
	int       drag_data_info;
	gboolean  drop_occured;
    gboolean  in_drag;
    gchar     *desktop_dnd_source_fs;
    gboolean  desktop_dnd_can_delete_source;

	GtkWidget *popup_menu;

	/* volume mounting - delayed open process */
	gboolean mounting;
	NemoWindowSlot *go_to_after_mount_slot;
	NemoWindowOpenFlags go_to_after_mount_flags;

	guint bookmarks_changed_id;

    gboolean my_computer_expanded;
    gboolean bookmarks_expanded;
    gboolean devices_expanded;
    gboolean network_expanded;

    gboolean updating_sidebar;

    /* Store the positions of the last
     * entry prior to bookmarks, and
     * the first entry after bookmarks
     * to allow drag and drop creation
     * of new bookmarks */
    gchar *top_bookend_uri;
    gchar *bottom_bookend_uri;

    gint bookmark_breakpoint;
    guint expand_timeout_source;
    guint popup_menu_action_index;
    guint update_places_on_idle_id;

    guint hidden_files_changed_id;   // filter hidden files/directories
	guint show_selection_idle_id;							 //
    gboolean use_file_treeview;      // switch file treeview functionality on/off

    NemoFile *popup_file;
    guint popup_file_idle_handler;

    guint selection_changed_timer;
    NemoFile *activation_file;
    NemoWindowOpenFlags activation_flags;
	NemoTreeViewDragDest *drag_dest;

} NemoPlacesSidebar;

typedef struct {
	GtkScrolledWindowClass parent;
} NemoPlacesSidebarClass;

typedef struct {
        GObject parent;
} NemoPlacesSidebarProvider;

typedef struct {
        GObjectClass parent;
} NemoPlacesSidebarProviderClass;

enum {
	PLACES_SIDEBAR_COLUMN_NAME,			// FM_TREE_MODEL_DISPLAY_NAME_COLUMN
	PLACES_SIDEBAR_COLUMN_GICON,         // FM_TREE_MODEL_CLOSED_ICON_COLUMN
	PLACES_SIDEBAR_OPEN_ICON_COLUMN,     // FM_TREE_MODEL_OPEN_ICON_COLUMN
	PLACES_SIDEBAR_FONT_STYLE_COLUMN,    // FM_TREE_MODEL_FONT_STYLE_COLUMN
    PLACES_SIDEBAR_TEXT_WEIGHT_COLUMN,   // FM_TREE_MODEL_TEXT_WEIGHT_COLUMN
	PLACES_SIDEBAR_COLUMN_ROW_TYPE,
	PLACES_SIDEBAR_COLUMN_URI,
	PLACES_SIDEBAR_COLUMN_DRIVE,
	PLACES_SIDEBAR_COLUMN_VOLUME,
	PLACES_SIDEBAR_COLUMN_MOUNT,
	PLACES_SIDEBAR_COLUMN_INDEX,
	PLACES_SIDEBAR_COLUMN_EJECT,
	PLACES_SIDEBAR_COLUMN_NO_EJECT,
	PLACES_SIDEBAR_COLUMN_BOOKMARK,
    PLACES_SIDEBAR_COLUMN_TOOLTIP,
    PLACES_SIDEBAR_COLUMN_EJECT_ICON,
	PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE,
	PLACES_SIDEBAR_COLUMN_SECTION_TYPE,
	PLACES_SIDEBAR_COLUMN_HEADING_TEXT,
    PLACES_SIDEBAR_COLUMN_DF_PERCENT,
    PLACES_SIDEBAR_COLUMN_SHOW_DF,
	PLACES_SIDEBAR_COLUMN_TREE_NAME,       // used for search in file tree view only
	PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum {
	PLACES_TREE_FOLDER,
	PLACES_BUILT_IN,
	PLACES_XDG_DIR,
	PLACES_MOUNTED_VOLUME,
	PLACES_BOOKMARK,
	PLACES_HEADING,
} PlaceType;

typedef enum {
    SECTION_COMPUTER,
    SECTION_XDG_BOOKMARKS,
    SECTION_BOOKMARKS,
	SECTION_DEVICES,
	SECTION_NETWORK,
} SectionType;

enum {
    POSITION_UPPER,
    POSITION_MIDDLE,
    POSITION_LOWER
};

// Polling-Intervall for network filesystems (ms)
#define POLL_INTERVAL 5000

typedef struct {
	NemoPlacesSidebar  *sidebar;
	GtkTreeRowReference *tree_ref;
	char *uri;
	NemoFile *file; 				  //
	GFileMonitor *monitor;
	GIcon *icon;
    GFile *dir;
    guint poll_id;
    GList *last_snapshot;
} TreeNodeData;

static void  open_selected_bookmark                    (NemoPlacesSidebar        *sidebar,
							GtkTreeModel                 *model,
							GtkTreeIter                  *iter,
							NemoWindowOpenFlags flags);
static void  nemo_places_sidebar_style_set         (GtkWidget                    *widget,
							GtkStyle                     *previous_style);
static gboolean eject_or_unmount_bookmark              (NemoPlacesSidebar *sidebar,
							GtkTreePath *path);
static gboolean eject_or_unmount_selection             (NemoPlacesSidebar *sidebar);
static void  check_unmount_and_eject                   (GMount *mount,
							GVolume *volume,
							GDrive *drive,
							gboolean *show_unmount,
							gboolean *show_eject);

static void update_places                              (NemoPlacesSidebar *sidebar);
static void update_places_on_idle                      (NemoPlacesSidebar *sidebar);
static void rebuild_menu                               (NemoPlacesSidebar *sidebar);
static void actions_changed                            (gpointer user_data);
static gchar *get_icon_name 						   (const gchar *uri);
static void refresh_highlight (NemoPlacesSidebar *sidebar);
static gboolean get_selected_iter (NemoPlacesSidebar *sidebar, GtkTreeIter *iter);
static gboolean get_selected_iter_for_store (NemoPlacesSidebar *sidebar, GtkTreeIter *iter);
static gboolean nemo_places_sidebar_row_draggable (GtkTreeDragSource *drag_source, GtkTreePath *path, gpointer user_data);

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry nemo_shortcuts_source_targets[] = {
	{ (char *)"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry nemo_shortcuts_drop_targets [] = {
	{ (char *)"GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
	{ (char *)"text/uri-list", 0, TEXT_URI_LIST }
};

/* Drag and drop interface declarations */
typedef struct {
  GtkTreeStore parent;

  NemoPlacesSidebar *sidebar;
} NemoShortcutsModel;

typedef struct {
  GtkTreeStoreClass parent_class;
} NemoShortcutsModelClass;

#define NEMO_TYPE_SHORTCUTS_MODEL (_nemo_shortcuts_model_get_type ())
#define NEMO_SHORTCUTS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_SHORTCUTS_MODEL_TYPE, NemoShortcutsModel))

GType _nemo_shortcuts_model_get_type (void);
static void _nemo_shortcuts_model_drag_source_init (GtkTreeDragSourceIface *iface);
G_DEFINE_TYPE_WITH_CODE (NemoShortcutsModel, _nemo_shortcuts_model, GTK_TYPE_TREE_STORE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						_nemo_shortcuts_model_drag_source_init));
static GtkTreeStore *nemo_shortcuts_model_new (NemoPlacesSidebar *sidebar);

G_DEFINE_TYPE (NemoPlacesSidebar, nemo_places_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static void
breakpoint_changed_cb (NemoPlacesSidebar *sidebar)
{
    sidebar->bookmark_breakpoint = g_settings_get_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT);
    update_places (sidebar);
}

static void
increment_bookmark_breakpoint (NemoPlacesSidebar *sidebar)
{
    g_signal_handlers_block_by_func (nemo_window_state, breakpoint_changed_cb, sidebar);

    sidebar->bookmark_breakpoint ++;
    g_settings_set_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT, sidebar->bookmark_breakpoint);

    g_signal_handlers_unblock_by_func (nemo_window_state, breakpoint_changed_cb, sidebar);
}

static void
decrement_bookmark_breakpoint (NemoPlacesSidebar *sidebar)
{
    g_signal_handlers_block_by_func (nemo_window_state, breakpoint_changed_cb, sidebar);

    sidebar->bookmark_breakpoint --;
    g_settings_set_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT, sidebar->bookmark_breakpoint);

    g_signal_handlers_unblock_by_func (nemo_window_state, breakpoint_changed_cb, sidebar);
}


static void
get_fm_tree_model_iter (GtkTreeModel *model,
                        GtkTreeIter  *iter,
                        GtkTreeIter  *fm_iter)
{
    GtkTreeIter tmp_iter = *iter;
    GtkTreeModel *child_model = model;

    // If filter model → pass to child model
    if (GTK_IS_TREE_MODEL_FILTER (model)) {
        gtk_tree_model_filter_convert_iter_to_child_iter (GTK_TREE_MODEL_FILTER (model),
                                                          &tmp_iter, iter);
        child_model = gtk_tree_model_filter_get_model (GTK_TREE_MODEL_FILTER (model));
    }

    // If sort model → go one step deeper
    if (GTK_IS_TREE_MODEL_SORT (child_model)) {
        gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (child_model),
                                                        fm_iter, &tmp_iter);
    } else {
        *fm_iter = tmp_iter;
    }
}

static char *
get_fm_tree_model_uri ( NemoPlacesSidebar *sidebar,
					   GtkTreeModel *model,
                        GtkTreeIter  *iter)
{
	if(!sidebar->use_file_treeview)return NULL;
	char *uri=NULL;
	if(uri==0) {
		GtkTreeIter fm_iter;
		get_fm_tree_model_iter (model, iter, &fm_iter);
		NemoFile *file = fm_tree_model_iter_get_file (FM_TREE_MODEL (sidebar->store), &fm_iter);
		if(file) {
		    uri = nemo_file_get_uri(file);
			nemo_file_unref(file);
		}
	}
	return uri;
}

static gboolean
should_show_desktop (void)
{
	return g_settings_get_boolean (nemo_desktop_preferences, NEMO_PREFERENCES_SHOW_DESKTOP) &&
	       !g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR);
}

static gboolean
is_built_in_bookmark (NemoFile *file)
{
	if (nemo_file_is_home (file)) {
		return TRUE;
	}

    if (nemo_file_is_desktop_directory (file) && should_show_desktop ())
        return TRUE;
    else
	    return FALSE;
}

static GtkTreeIter
add_heading (NemoPlacesSidebar *sidebar,
	     SectionType section_type,
	     const gchar *title)
{
	GtkTreeIter cat_iter;
	if(sidebar->use_file_treeview) {
		if (!fm_tree_model_append_head_root_node(sidebar->store, title, &cat_iter)) {
		    g_warning("Failed to append head root node");
		    return cat_iter; // You must return an invalid iter
		}
		fm_tree_model_set (sidebar->store, &cat_iter,
					PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_HEADING,
					PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
					PLACES_SIDEBAR_COLUMN_HEADING_TEXT, title,
		            PLACES_SIDEBAR_COLUMN_INDEX, -1,
					PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
					PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
					-1);

	} else {
		gtk_tree_store_append (sidebar->store, &cat_iter, NULL);
		gtk_tree_store_set (sidebar->store, &cat_iter,
					PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_HEADING,
					PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
					PLACES_SIDEBAR_COLUMN_HEADING_TEXT, title,
		            PLACES_SIDEBAR_COLUMN_INDEX, -1,
					PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
					PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
					-1);
	}
	return cat_iter;
}

static GtkTreeIter
check_heading_for_devices (NemoPlacesSidebar *sidebar,
                           SectionType section_type,
			               GtkTreeIter cat_iter)
{
    if (section_type == SECTION_DEVICES) {
        if (!sidebar->devices_header_added) {
            cat_iter = add_heading (sidebar, SECTION_DEVICES,
                        _("Devices"));
            sidebar->devices_header_added = TRUE;
        }
    }
    return cat_iter;
}

typedef struct {
       PlaceType place_type;
       SectionType section_type;
       gchar *name;
       gchar *icon_name;
       gchar *uri;
       GDrive *drive;
       GVolume *volume;
       GMount *mount;
       gint index;
       gchar *tooltip;
       gint df_percent;
       gboolean show_df_percent;
} PlaceInfo;

static gint
sort_places_func (gconstpointer a,
                  gconstpointer b)
{
    return g_utf8_collate (((PlaceInfo *) a)->name, ((PlaceInfo *) b)->name);
}

static PlaceInfo *
new_place_info (PlaceType place_type,
                SectionType section_type,
                gchar *name,
                gchar *icon_name,
                gchar *uri,
                GDrive *drive,
                GVolume *volume,
                GMount *mount,
                gint index,
                gchar *tooltip,
                gint df_percent,
                gboolean show_df_percent)
{
    PlaceInfo *info = g_new0 (PlaceInfo, 1);

    info->place_type = place_type;
    info->section_type = section_type;
    info->name = g_utf8_make_valid (name, -1);
    info->icon_name = g_strdup (icon_name);
    info->uri = (g_strdup (uri));
    info->drive = drive ? g_object_ref (drive) : NULL;
    info->volume = volume ? g_object_ref (volume) : NULL;
    info->mount = mount ? g_object_ref (mount) : NULL;
    info->index = index;
    info->tooltip = g_strdup (tooltip);
    info->df_percent = df_percent;
    info->show_df_percent = show_df_percent;

    return info;
}

static void
free_place_info (PlaceInfo *info)
{
    g_clear_pointer(&info->name, g_free);
    g_clear_pointer(&info->icon_name, g_free);
    g_clear_pointer(&info->uri, g_free);
    g_clear_object (&info->drive);
    g_clear_object (&info->volume);
    g_clear_object (&info->mount);
    g_clear_pointer(&info->tooltip, g_free);

    g_free (info);
}

static GtkTreeIter
add_place (NemoPlacesSidebar *sidebar,
	   PlaceType place_type,
	   SectionType section_type,
	   const char *name,
	   const char *icon_name,
	   const char *uri,
	   GDrive *drive,
	   GVolume *volume,
	   GMount *mount,
	   int index,
	   const char *tooltip,
       int df_percent,
       gboolean show_df_percent,
       GtkTreeIter cat_iter)
{
	GtkTreeIter           iter;
    GIcon *gicon;
	gboolean show_eject, show_unmount;
	gboolean show_eject_button;

	cat_iter = check_heading_for_devices (sidebar, section_type, cat_iter);

	check_unmount_and_eject (mount, volume, drive,
				 &show_unmount, &show_eject);

	if (show_unmount || show_eject) {
		g_assert (place_type != PLACES_BOOKMARK);
	}

	if (mount == NULL) {
		show_eject_button = FALSE;
	} else {
		show_eject_button = (show_unmount || show_eject);
	}

    gicon = (icon_name != NULL) ? g_themed_icon_new (icon_name) : NULL;
    if(sidebar->use_file_treeview) {
			if(uri) {
				fm_tree_model_add_root_uri_head (sidebar->store, uri, &cat_iter , &iter, name ,
									 gicon ? g_object_ref(gicon) : NULL, mount ? g_object_ref(mount) : NULL);
			} else {
				fm_tree_model_append_child_node (sidebar->store, &cat_iter, &iter);
			}
		    fm_tree_model_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_GICON, gicon ? g_object_ref(gicon) : NULL,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_DRIVE, drive ? g_object_ref(drive) : NULL,
			    PLACES_SIDEBAR_COLUMN_VOLUME, volume ? g_object_ref(volume) : NULL,
			    PLACES_SIDEBAR_COLUMN_MOUNT, mount ? g_object_ref(mount) : NULL,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, index,
			    PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
			    PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
			    PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
                PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
                PLACES_SIDEBAR_COLUMN_EJECT_ICON, show_eject_button ? "xsi-media-eject-symbolic" : NULL,
			    PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE, EJECT_ICON_SIZE_NOT_HOVERED,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
                PLACES_SIDEBAR_COLUMN_DF_PERCENT, df_percent,
                PLACES_SIDEBAR_COLUMN_SHOW_DF, show_df_percent,
			    -1);
	} else {
		gtk_tree_store_append (sidebar->store, &iter, &cat_iter);
		gtk_tree_store_set (sidebar->store, &iter,
					PLACES_SIDEBAR_COLUMN_GICON, gicon ? g_object_ref(gicon) : NULL,
					PLACES_SIDEBAR_COLUMN_NAME, name,
					PLACES_SIDEBAR_COLUMN_URI, uri,
					PLACES_SIDEBAR_COLUMN_DRIVE, drive ? g_object_ref(drive) : NULL,
					PLACES_SIDEBAR_COLUMN_VOLUME, volume ? g_object_ref(volume) : NULL,
					PLACES_SIDEBAR_COLUMN_MOUNT, mount ? g_object_ref(mount) : NULL,
					PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
					PLACES_SIDEBAR_COLUMN_INDEX, index,
					PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
					PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
					PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
		            PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
		            PLACES_SIDEBAR_COLUMN_EJECT_ICON, show_eject_button ? "media-eject-symbolic" : NULL,
					PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE, EJECT_ICON_SIZE_NOT_HOVERED,
					PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
		            PLACES_SIDEBAR_COLUMN_DF_PERCENT, df_percent,
		            PLACES_SIDEBAR_COLUMN_SHOW_DF, show_df_percent,
					-1);
	}

	g_object_unref(gicon);

    return cat_iter;
}

typedef struct {
	const gchar *location;
	const gchar *last_uri;
	NemoPlacesSidebar *sidebar;
	GtkTreePath *path;
} RestoreLocationData;

static gboolean
restore_selection_foreach (GtkTreeModel *model,
			   GtkTreePath *path,
			   GtkTreeIter *iter,
			   gpointer user_data)
{
	RestoreLocationData *data = user_data;
	gchar *uri=NULL;
	if (FM_IS_TREE_MODEL(model)) {
		NemoFile *file = fm_tree_model_iter_get_file (FM_TREE_MODEL (model), iter);
		if(file) {
			uri = nemo_file_get_uri(file);
			if(uri) {
				if (g_strcmp0 (uri, data->last_uri) == 0 ||
					g_strcmp0 (uri, data->location) == 0) {
					data->path = gtk_tree_path_copy (path);
				}
				g_free (uri);
				nemo_file_unref (file);
				return (data->path != NULL);
			}
			nemo_file_unref (file);
		}
	}
	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_URI, &uri,
			    -1);
	if (g_strcmp0 (uri, data->last_uri) == 0 ||
	    g_strcmp0 (uri, data->location) == 0) {
		data->path = gtk_tree_path_copy (path);
	}

	if(uri)g_free (uri);

	return (data->path != NULL);
}

static gboolean
restore_expand_state_foreach (GtkTreeModel *model,
                              GtkTreePath *path,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    PlaceType place_type;
    SectionType section_type;
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    gtk_tree_model_get (model, iter,
                PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                -1);

    if (place_type == PLACES_HEADING) {
        if (section_type == SECTION_COMPUTER) {
            if (sidebar->my_computer_expanded)
                gtk_tree_view_expand_to_path (sidebar->tree_view, path);
            else
                gtk_tree_view_collapse_row (sidebar->tree_view, path);
        } else if (section_type == SECTION_BOOKMARKS) {
            if (sidebar->bookmarks_expanded)
                gtk_tree_view_expand_to_path (sidebar->tree_view, path);
            else
                gtk_tree_view_collapse_row (sidebar->tree_view, path);
        } else if (section_type == SECTION_DEVICES) {
            if (sidebar->devices_expanded)
                gtk_tree_view_expand_to_path (sidebar->tree_view, path);
            else
                gtk_tree_view_collapse_row (sidebar->tree_view, path);
        } else if (section_type == SECTION_NETWORK) {
            if (sidebar->network_expanded)
                gtk_tree_view_expand_to_path (sidebar->tree_view, path);
            else
                gtk_tree_view_collapse_row (sidebar->tree_view, path);
        }
    }
    return FALSE;
}

static void
restore_expand_state (NemoPlacesSidebar *sidebar)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    gtk_tree_model_foreach (model,
                restore_expand_state_foreach, sidebar);
}


static void expand_or_collapse_category (NemoPlacesSidebar *sidebar,
                             SectionType section_type, gboolean expand)
{
    switch (section_type) {
        case SECTION_COMPUTER:
            sidebar->my_computer_expanded = expand;
            break;
        case SECTION_BOOKMARKS:
            sidebar->bookmarks_expanded = expand;
            break;
        case SECTION_DEVICES:
            sidebar->devices_expanded = expand;
            break;
        case SECTION_NETWORK:
            sidebar->network_expanded = expand;
            break;
        case SECTION_XDG_BOOKMARKS:
        default:
            break;
    }

    restore_expand_state (sidebar);
}

static void
sidebar_update_restore_selection (NemoPlacesSidebar *sidebar,
				  const gchar *location,
				  const gchar *last_uri)
{
	RestoreLocationData data;
	GtkTreeSelection *selection;

	data.location = location;
	data.last_uri = last_uri;
	data.sidebar = sidebar;
	data.path = NULL;

	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_model_foreach (model,
				restore_selection_foreach, &data);

	if (data.path != NULL) {
		selection = gtk_tree_view_get_selection (sidebar->tree_view);
		gtk_tree_selection_select_path (selection, data.path);
		gtk_tree_path_free (data.path);
	}
}

static gint
get_disk_full (GFile *file, gchar **tooltip_info)
{
    GFileInfo *info;
    GError *error;
    guint64 k_used, k_total, k_free;
    gint df_percent;
    float fraction;
    int prefix;
    gchar *size_string;
    gchar *out_string;

    error = NULL;
    df_percent = -1;
    out_string = NULL;

    info = g_file_query_filesystem_info (file,
                                         "filesystem::*",
                                         NULL,
                                         &error);

    if (info != NULL) {
        k_used = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USED);
        k_total = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        k_free = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

        if (k_total > 0) {
            fraction = ((float) k_used / (float) k_total) * 100.0;

            df_percent = (gint) rintf(fraction);

            prefix = nemo_global_preferences_get_size_prefix_preference ();
            size_string = g_format_size_full (k_free, prefix);

            out_string = g_strdup_printf (_("Free space: %s"), size_string);

            g_free (size_string);
        }

        g_object_unref (info);
    }

    if (error != NULL) {
        g_warning ("Couldn't get disk full info for: %s", error->message);
        g_clear_error (&error);
    }

    if (out_string == NULL) {
        out_string = g_strdup (" ");
    }

    *tooltip_info = out_string;

    return df_percent;
}

static gboolean
home_on_different_fs (const gchar *home_uri)
{
    GFile *home = g_file_new_for_uri (home_uri);
    GFile *root = g_file_new_for_uri ("file:///");
    GFileInfo *home_info, *root_info;
    const gchar *home_id, *root_id;
    gboolean res;

    res = FALSE;
    home_info = g_file_query_info (home,
                                   G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   NULL, NULL);
    root_info = g_file_query_info (root,
                                   G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   NULL, NULL);

    if (home_info && root_info) {
        home_id = g_file_info_get_attribute_string (home_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        root_id = g_file_info_get_attribute_string (root_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        res = g_strcmp0 (home_id, root_id) != 0;
        g_object_unref (home_info);
        g_object_unref (root_info);
    } else {
        if (home_info)
            g_object_unref (home_info);
        if (root_info)
            g_object_unref (root_info);
    }
    g_object_unref (home);
    g_object_unref (root);
    return res;
}

static gchar *
get_icon_name (const gchar *uri)
{
    NemoFile *file = nemo_file_get_by_uri (uri);
    gchar *icon_name;

    icon_name = nemo_file_get_control_icon_name (file);
    nemo_file_unref (file);

    return icon_name;
}

static void
update_filtering_from_preferences (NemoPlacesSidebar *sidebar)
{
    NemoWindowShowHiddenFilesMode mode;

    if (sidebar->store == NULL) {
        return;
    }

    mode = nemo_window_get_hidden_files_mode (sidebar->window);

    fm_tree_model_set_show_hidden_files (sidebar->store,
                                         mode == NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE);

    fm_tree_model_set_show_only_directories (sidebar->store,TRUE);
}

static void tree_model_remove_all_nodes (NemoPlacesSidebar *sidebar)
{
	if(sidebar->use_file_treeview) {
		fm_tree_model_remove_all_nodes( (FMTreeModel*) (sidebar->store) );
	} else {
		gtk_tree_store_clear (GTK_TREE_STORE (sidebar->store));
	}
}
static void
update_places (NemoPlacesSidebar *sidebar)
{
	NemoBookmark *bookmark;
	GtkTreeIter last_iter, cat_iter;
	GVolumeMonitor *volume_monitor;
	GList *mounts, *l, *ll;
	GMount *mount;
	GList *drives;
	GDrive *drive;
	GList *volumes;
	GVolume *volume;
	int bookmark_count, bookmark_index;
	char *location, *mount_uri, *name, *desktop_path, *last_uri, *identifier;
	const gchar *bookmark_name;
	gchar *icon;
	GFile *root, *df_file;
	NemoWindowSlot *slot;
	char *tooltip;
    gchar *tooltip_info;
	GList *network_mounts, *network_volumes;
    gint full;

	DEBUG ("Updating places sidebar");

    sidebar->updating_sidebar = TRUE;

	if(sidebar->store == NULL) return;
	last_uri = NULL;

    g_clear_pointer (&sidebar->top_bookend_uri, g_free);
    g_clear_pointer (&sidebar->bottom_bookend_uri, g_free);

	if (get_selected_iter (sidebar, &last_iter)) {
		GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
		gtk_tree_model_get (GTK_TREE_MODEL (model),
					&last_iter,
					PLACES_SIDEBAR_COLUMN_URI, &last_uri, -1);
		if(last_uri == NULL) {
			last_uri = get_fm_tree_model_uri (sidebar, model, &last_iter);
		}
	}
	tree_model_remove_all_nodes(sidebar);

	sidebar->devices_header_added = FALSE;
	sidebar->bookmarks_header_added = FALSE;

	slot = nemo_window_get_active_slot (sidebar->window);
	location = nemo_window_slot_get_current_uri (slot);

	network_mounts = network_volumes = NULL;
	volume_monitor = sidebar->volume_monitor;

	if(sidebar->use_file_treeview) {
	    update_filtering_from_preferences (sidebar);
	}

    cat_iter = add_heading (sidebar, SECTION_COMPUTER,
                                    _("My Computer"));
    /* add built in bookmarks */

    /* home folder */
    mount_uri = nemo_get_home_directory_uri ();
    icon = get_icon_name (mount_uri);

    df_file = g_file_new_for_uri (mount_uri);
    full = get_disk_full (df_file, &tooltip_info);
    g_clear_object (&df_file);

    tooltip = g_strdup_printf (_("Open your personal folder\n%s"), tooltip_info);
    g_free (tooltip_info);
    cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_COMPUTER,
                           _("Home"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           tooltip,
                           full, home_on_different_fs (mount_uri) && full > -1,
                           cat_iter);
    g_free (icon);
    sidebar->top_bookend_uri = g_strdup (mount_uri);
    g_free (mount_uri);
    g_free (tooltip);

    if (should_show_desktop ()) {
        /* desktop */
        desktop_path = nemo_get_desktop_directory ();
        mount_uri = g_filename_to_uri (desktop_path, NULL, NULL);
        icon = get_icon_name (mount_uri);
        cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                               SECTION_COMPUTER,
                               _("Desktop"), icon,
                               mount_uri, NULL, NULL, NULL, 0,
                               _("Open the contents of your desktop in a folder"), 0, FALSE,
                               cat_iter);
        g_free (icon);
        g_free (sidebar->top_bookend_uri);
        sidebar->top_bookend_uri = g_strdup (mount_uri);
        g_free (mount_uri);
        g_free (desktop_path);
    }

    /* add bookmarks */
    bookmark_count = nemo_bookmark_list_length (sidebar->bookmarks);
    /* in certain situations (i.e. removed a bookmark), the breakpoint is smaller than
     * the number of bookmarks - make sure to fix this before iterating through a list of them.
     * We don't overwrite the stored breakpoint because the bookmark list could simply be reloading,
     * and we want the original number still when we update places again.
     */
    gint temp_breakpoint = sidebar->bookmark_breakpoint;

    if (temp_breakpoint < 0 ||
        temp_breakpoint > bookmark_count) {
        temp_breakpoint = bookmark_count;
    }

    for (bookmark_index = 0; bookmark_index < temp_breakpoint; ++bookmark_index) {
        bookmark = nemo_bookmark_list_item_at (sidebar->bookmarks, bookmark_index);

        root = nemo_bookmark_get_location (bookmark);

        bookmark_name = nemo_bookmark_get_name (bookmark);
        icon = nemo_bookmark_get_icon_name (bookmark);
        mount_uri = nemo_bookmark_get_uri (bookmark);
        tooltip = g_file_get_parse_name (root);

        cat_iter = add_place (sidebar, PLACES_BOOKMARK,
                               SECTION_XDG_BOOKMARKS,
                               bookmark_name, icon, mount_uri,
                               NULL, NULL, NULL, bookmark_index,
                               tooltip, 0, FALSE,
                               cat_iter);
        g_object_unref (root);
        g_free (icon);
        g_free (mount_uri);
        g_free (tooltip);
    }

    if (eel_vfs_supports_uri_scheme ("favorites")) {
        gint n = xapp_favorites_get_n_favorites (xapp_favorites_get_default ());

        if (n > 0) {
            mount_uri = (char *)"favorites:///"; /* No need to strdup */
            icon = "xsi-user-favorites-symbolic";
            cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                                  SECTION_COMPUTER,
                                  _("Favorites"), icon, mount_uri,
                                  NULL, NULL, NULL, 0,
                                  _("Favorite files"), 0, FALSE, cat_iter);

            sidebar->bottom_bookend_uri = g_strdup (mount_uri);
        }
    }

    gboolean recent_enabled;
    recent_enabled = g_settings_get_boolean (cinnamon_privacy_preferences,
                                             NEMO_PREFERENCES_RECENT_ENABLED);

    if (recent_enabled && eel_vfs_supports_uri_scheme ("recent")) {
        mount_uri = (char *)"recent:///"; /* No need to strdup */
        icon = NEMO_ICON_SYMBOLIC_FOLDER_RECENT;
        cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                              SECTION_COMPUTER,
                              _("Recent"), icon, mount_uri,
                              NULL, NULL, NULL, 0,
                              _("Recent files"), 0, FALSE, cat_iter);

        if (sidebar->bottom_bookend_uri == NULL) {
            sidebar->bottom_bookend_uri = g_strdup (mount_uri);
        }
    }

    /* file system root */
    mount_uri = (char *)"file:///"; /* No need to strdup */
    icon = NEMO_ICON_SYMBOLIC_FILESYSTEM;

    df_file = g_file_new_for_uri (mount_uri);
    full = get_disk_full (df_file, &tooltip_info);
    g_clear_object (&df_file);

    tooltip = g_strdup_printf (_("Open the contents of the File System\n%s"), tooltip_info);
    g_free (tooltip_info);
    cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                           SECTION_COMPUTER,
                           _("File System"), icon,
                           mount_uri, NULL, NULL, NULL, 0,
                           tooltip,
                           full, full > -1,
                           cat_iter);
    g_free (tooltip);

    if (sidebar->bottom_bookend_uri == NULL) {
        sidebar->bottom_bookend_uri = g_strdup (mount_uri);
    }

    if (eel_vfs_supports_uri_scheme("trash")) {
        mount_uri = (char *)"trash:///"; /* No need to strdup */
        icon = nemo_trash_monitor_get_symbolic_icon_name ();
        cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                               SECTION_COMPUTER,
                               _("Trash"), icon, mount_uri,
                               NULL, NULL, NULL, 0,
                               _("Open the trash"), 0, FALSE,
                               cat_iter);
        g_free (icon);
    }

    cat_iter = add_heading (sidebar, SECTION_BOOKMARKS,
                                    _("Bookmarks"));

    while (bookmark_index < bookmark_count) {
        bookmark = nemo_bookmark_list_item_at (sidebar->bookmarks, bookmark_index);

        root = nemo_bookmark_get_location (bookmark);

        bookmark_name = nemo_bookmark_get_name (bookmark);
        icon = nemo_bookmark_get_icon_name (bookmark);
        mount_uri = nemo_bookmark_get_uri (bookmark);
        tooltip = g_file_get_parse_name (root);

        cat_iter = add_place (sidebar, PLACES_BOOKMARK,
                              SECTION_BOOKMARKS,
                              bookmark_name, icon, mount_uri,
                              NULL, NULL, NULL, bookmark_index,
                              tooltip, 0, FALSE,
                              cat_iter);
        g_object_unref (root);
        g_free (icon);
        g_free (mount_uri);
        g_free (tooltip);
        ++bookmark_index;
    }

    GList *place_infos = NULL;
    PlaceInfo *place_info;

    /* add mounts that has no volume (/etc/mtab mounts, ftp, sftp,...) */
    mounts = g_volume_monitor_get_mounts (volume_monitor);

    for (l = mounts; l != NULL; l = l->next) {
        mount = l->data;
        if (g_mount_is_shadowed (mount)) {
            g_object_unref (mount);
            continue;
        }
        volume = g_mount_get_volume (mount);
        if (volume != NULL) {
            g_object_unref (volume);
            g_object_unref (mount);
            continue;
        }
        root = g_mount_get_default_location (mount);

        if (!g_file_is_native (root)) {
            gboolean really_network = TRUE;
            gchar *path = g_file_get_path (root);
            if (!path) {
                network_mounts = g_list_prepend (network_mounts, mount);
                g_object_unref (root);
                continue;
            }
            gchar *escaped1 = g_uri_unescape_string (path, "");
            gchar *escaped2 = g_uri_unescape_string (escaped1, "");
            gchar *ptr = g_strrstr (escaped2, "file://");
            if (ptr != NULL) {
                GFile *actual_file = g_file_new_for_uri (ptr);
                if (g_file_is_native(actual_file)) {
                    really_network = FALSE;
                }
                g_object_unref(actual_file);
            }
            g_free (path);
            g_free (escaped1);
            g_free (escaped2);
            if (really_network) {
                network_mounts = g_list_prepend (network_mounts, mount);
                g_object_unref (root);
                continue;
            }
        }
        g_object_unref (root);
        g_object_unref (mount);
    }
    g_list_free (mounts);

    /* first go through all connected drives */
    drives = g_volume_monitor_get_connected_drives (volume_monitor);

    for (l = drives; l != NULL; l = l->next) {
        drive = l->data;

        volumes = g_drive_get_volumes (drive);
        if (volumes != NULL) {
            for (ll = volumes; ll != NULL; ll = ll->next) {
                volume = ll->data;
                identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

                if (g_strcmp0 (identifier, "network") == 0) {
                    g_free (identifier);
                    network_volumes = g_list_prepend (network_volumes, volume);
                    continue;
                }
                g_free (identifier);

                mount = g_volume_get_mount (volume);
                if (mount != NULL) {
                    if (g_mount_is_shadowed (mount)) {
                        g_object_unref (mount);
                        g_object_unref (volume);
                        continue;
                    }

                    gchar *full_display_name, *volume_id;
                    /* Show mounted volume in the sidebar */
                    icon = nemo_get_mount_icon_name (mount);
                    root = g_mount_get_default_location (mount);
                    mount_uri = g_file_get_uri (root);
                    name = g_mount_get_name (mount);

                    volume_id = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
                    full_display_name = g_file_get_parse_name (root);

                    df_file = g_file_new_for_uri (mount_uri);
                    full = get_disk_full (df_file, &tooltip_info);
                    g_clear_object (&df_file);

                    tooltip = g_strdup_printf (_("%s (%s)\n%s"),
                                               full_display_name,
                                               volume_id,
                                               tooltip_info);
                    g_free (tooltip_info);
                    place_info = new_place_info (PLACES_MOUNTED_VOLUME,
                                                 SECTION_DEVICES,
                                                 name, icon, mount_uri,
                                                 drive, volume, mount, 0, tooltip, full, full > -1);
                    place_infos = g_list_prepend (place_infos, place_info);
                    g_object_unref (root);
                    g_object_unref (mount);
                    g_free (icon);
                    g_free (tooltip);
                    g_free (name);
                    g_free (mount_uri);
                    g_free (volume_id);
                    g_free (full_display_name);
                } else {
                    /* Do show the unmounted volumes in the sidebar;
                     * this is so the user can mount it (in case automounting
                     * is off).
                     *
                     * Also, even if automounting is enabled, this gives a visual
                     * cue that the user should remember to yank out the media if
                     * he just unmounted it.
                     */
                    gchar *volume_id;
                    icon = nemo_get_volume_icon_name (volume);
                    name = g_volume_get_name (volume);

                    volume_id = g_volume_get_identifier (volume,
                                                         G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
                    tooltip = g_strdup_printf (_("Mount and open %s (%s)"), name, volume_id);

                    place_info = new_place_info (PLACES_MOUNTED_VOLUME,
                                                 SECTION_DEVICES,
                                                 name, icon, NULL,
                                                 drive, volume, NULL, 0, tooltip, 0, FALSE);
                    place_infos = g_list_prepend (place_infos, place_info);

                    g_free (icon);
                    g_free (name);
                    g_free (tooltip);
                    g_free (volume_id);
                }
                g_object_unref (volume);
            }
            g_list_free (volumes);
        } else {
            if (g_drive_is_removable (drive) && !g_drive_is_media_check_automatic (drive)) {
                /* If the drive has no mountable volumes and we cannot detect media change.. we
                 * display the drive in the sidebar so the user can manually poll the drive by
                 * right clicking and selecting "Rescan..."
                 *
                 * This is mainly for drives like floppies where media detection doesn't
                 * work.. but it's also for human beings who like to turn off media detection
                 * in the OS to save battery juice.
                 */
                icon = nemo_get_drive_icon_name (drive);
                name = g_drive_get_name (drive);
                tooltip = g_strdup_printf (_("Mount and open %s"), name);

                place_info = new_place_info (PLACES_BUILT_IN,
                                             SECTION_DEVICES,
                                             name, icon, NULL,
                                             drive, NULL, NULL, 0, tooltip, 0, FALSE);
                place_infos = g_list_prepend (place_infos, place_info);

                g_free (icon);
                g_free (tooltip);
                g_free (name);
            }
        }
        g_object_unref (drive);
    }
    g_list_free (drives);

    /* add all volumes that is not associated with a drive */
    volumes = g_volume_monitor_get_volumes (volume_monitor);
    for (l = volumes; l != NULL; l = l->next) {
        volume = l->data;
        drive = g_volume_get_drive (volume);
        if (drive != NULL) {
                g_object_unref (volume);
            g_object_unref (drive);
            continue;
        }

        identifier = g_volume_get_identifier (volume, G_VOLUME_IDENTIFIER_KIND_CLASS);

        if (g_strcmp0 (identifier, "network") == 0) {
            g_free (identifier);
            network_volumes = g_list_prepend (network_volumes, volume);
            continue;
        }
        g_free (identifier);

        mount = g_volume_get_mount (volume);
        if (mount != NULL) {
            if (g_mount_is_shadowed (mount)) {
                g_object_unref (mount);
                g_object_unref (volume);
                continue;
            }

            g_autofree gchar *parse_name = NULL;
            icon = nemo_get_mount_icon_name (mount);
            root = g_mount_get_default_location (mount);
            mount_uri = g_file_get_uri (root);

            df_file = g_file_new_for_uri (mount_uri);
            full = get_disk_full (df_file, &tooltip_info);
            g_clear_object (&df_file);

            parse_name = g_file_get_parse_name (root);
            tooltip = g_strdup_printf (_("%s\n%s"), parse_name, tooltip_info);

            g_free (tooltip_info);
            g_object_unref (root);
            name = g_mount_get_name (mount);

            place_info = new_place_info (PLACES_MOUNTED_VOLUME,
                                         SECTION_DEVICES,
                                         name, icon, mount_uri,
                                         NULL, volume, mount, 0, tooltip, full, full > -1);
            place_infos = g_list_prepend (place_infos, place_info);

            g_object_unref (mount);
            g_free (icon);
            g_free (name);
            g_free (tooltip);
            g_free (mount_uri);
        } else {
            /* see comment above in why we add an icon for an unmounted mountable volume */
            icon = nemo_get_volume_icon_name (volume);
            name = g_volume_get_name (volume);

            place_info = new_place_info (PLACES_MOUNTED_VOLUME,
                                         SECTION_DEVICES,
                                         name, icon, NULL,
                                         NULL, volume, NULL, 0, name, 0, FALSE);
            place_infos = g_list_prepend (place_infos, place_info);

            g_free (icon);
            g_free (name);
        }
        g_object_unref (volume);
    }
    g_list_free (volumes);

    place_infos = g_list_sort (place_infos, (GCompareFunc) sort_places_func);

    for (l = place_infos; l != NULL; l = l->next) {
        PlaceInfo *info = (PlaceInfo *) l->data;

        cat_iter = add_place (sidebar,
                              info->place_type,
                              info->section_type,
                              info->name,
                              info->icon_name,
                              info->uri,
                              info->drive,
                              info->volume,
                              info->mount,
                              info->index,
                              info->tooltip,
                              info->df_percent,
                              info->show_df_percent,
                              cat_iter);

        free_place_info (info);
    }

    g_list_free (place_infos);

	/* network */
	cat_iter = add_heading (sidebar, SECTION_NETWORK,
		     _("Network"));

	network_volumes = g_list_reverse (network_volumes);
	for (l = network_volumes; l != NULL; l = l->next) {
		volume = l->data;
		mount = g_volume_get_mount (volume);

		if (mount != NULL) {
			network_mounts = g_list_prepend (network_mounts, mount);
			continue;
		} else {
			icon = nemo_get_volume_icon_name (volume);
			name = g_volume_get_name (volume);
			tooltip = g_strdup_printf (_("Mount and open %s"), name);

			cat_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                				   SECTION_NETWORK,
                				   name, icon, NULL,
                				   NULL, volume, NULL, 0, tooltip, 0, FALSE,
                                   cat_iter);
			g_free (icon);
			g_free (name);
			g_free (tooltip);
		}
	}

	g_list_free_full (network_volumes, g_object_unref);

	network_mounts = g_list_reverse (network_mounts);
	for (l = network_mounts; l != NULL; l = l->next) {
		mount = l->data;
		root = g_mount_get_default_location (mount);
		icon = nemo_get_mount_icon_name (mount);
		mount_uri = g_file_get_uri (root);
		name = g_mount_get_name (mount);
		tooltip = g_file_get_parse_name (root);
		cat_iter = add_place (sidebar, PLACES_MOUNTED_VOLUME,
                			   SECTION_NETWORK,
                			   name, icon, mount_uri,
                			   NULL, NULL, mount, 0, tooltip, 0, FALSE,
                               cat_iter);
		g_object_unref (root);
		g_free (icon);
		g_free (name);
		g_free (mount_uri);
		g_free (tooltip);
	}

	g_list_free_full (network_mounts, g_object_unref);

	/* network:// */
 	mount_uri = (char *)"network:///"; /* No need to strdup */
	icon = NEMO_ICON_SYMBOLIC_NETWORK;
	cat_iter = add_place (sidebar, PLACES_BUILT_IN,
                		   SECTION_NETWORK,
                		   _("Network"), icon,
                		   mount_uri, NULL, NULL, NULL, 0,
                		   _("Browse the contents of the network"), 0, FALSE,
                           cat_iter);

	/* restore selection */
    restore_expand_state (sidebar);
	sidebar_update_restore_selection (sidebar, location, last_uri);

    actions_changed (sidebar);

    sidebar->updating_sidebar = FALSE;

	g_free (location);
	g_free (last_uri);
}



static gboolean
show_iter_for_file (NemoPlacesSidebar *sidebar, NemoFile *file, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	NemoFile *parent_file;
	GtkTreeIter parent_iter;
	GtkTreePath *path, *sort_path;
	GtkTreeIter cur_iter;

	if (sidebar->store == NULL) {
		return FALSE;
	}
	model = GTK_TREE_MODEL (sidebar->store);

	/* check if file is visible in the same root as the currently selected folder is */
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);
	if (path != NULL) {
		if (gtk_tree_model_get_iter (model, &cur_iter, path) &&
		    fm_tree_model_file_get_iter (sidebar->store, iter,
						 file, &cur_iter)) {
			gtk_tree_path_free (path);
			return TRUE;
		}
		gtk_tree_path_free (path);
	}
	/* check if file is visible at all */
	if (fm_tree_model_file_get_iter (sidebar->store,
					       iter, file, NULL)) {
		return TRUE;
	}

	parent_file = nemo_file_get_parent (file);

	if (parent_file == NULL) {
		return FALSE;
	}
	if (!show_iter_for_file (sidebar, parent_file, &parent_iter)) {
		nemo_file_unref (parent_file);
		return FALSE;
	}
	nemo_file_unref (parent_file);

	if (parent_iter.user_data == NULL || parent_iter.stamp == 0) {
		return FALSE;
	}
	path = gtk_tree_model_get_path (model, &parent_iter);
	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		( GTK_TREE_MODEL_SORT(sidebar->sort_model), path);
	gtk_tree_path_free (path);
	gtk_tree_view_expand_row (sidebar->tree_view, sort_path, FALSE);
	gtk_tree_path_free (sort_path);

	return FALSE;
}

static gboolean
show_selection_idle_callback (gpointer callback_data)
{
	NemoPlacesSidebar *sidebar;

	NemoFile *file, *old_file;
	GtkTreeIter iter;
	GtkTreePath *path, *sort_path=NULL;

	sidebar = NEMO_PLACES_SIDEBAR (callback_data);

	sidebar->show_selection_idle_id = 0;

	file = nemo_file_get_by_uri (sidebar->selection_location);
	if (file == NULL) {
		return FALSE;
	}

	if (!nemo_file_is_directory (file)) {
		old_file = file;
		file = nemo_file_get_parent (file);
		nemo_file_unref (old_file);
		if (file == NULL) {
			return FALSE;
		}
	}

	sidebar->selecting = TRUE;
	if (!show_iter_for_file (sidebar, file, &iter)) {
		nemo_file_unref (file);
		return FALSE;
	}
	sidebar->selecting = FALSE;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
	if (path == NULL) {
	    g_warning("Cannot get child path from iter");
	    return FALSE;
	}
	// The empty path [] refers to top_root_node or dummy — skip
	if (gtk_tree_path_get_depth(path) == 0) {
	    g_debug("Skipping sort conversion for invisible top_root_node");
	    gtk_tree_path_free(path);
	    return FALSE;
	}

	g_assert(sidebar->sort_model != NULL);
	g_assert(sidebar->store != NULL);
	GtkTreeModel *real_child = gtk_tree_model_sort_get_model(GTK_TREE_MODEL_SORT (sidebar->sort_model));
	g_assert(real_child == GTK_TREE_MODEL(sidebar->store));

	sort_path = gtk_tree_model_sort_convert_child_path_to_path
		(GTK_TREE_MODEL_SORT (sidebar->sort_model), path);
if (!sort_path) {
   // g_warning("sort_path == NULL for path %s", gtk_tree_path_to_string(path));

	gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
	if (gtk_widget_get_realized (GTK_WIDGET (sidebar->tree_view))) {
		gtk_tree_view_scroll_to_cell (sidebar->tree_view, path, NULL, FALSE, 0, 0);
	}
    gtk_tree_path_free(path);
    return FALSE;
}

	gtk_tree_path_free (path);
	gtk_tree_view_set_cursor (sidebar->tree_view, sort_path, NULL, FALSE);
	if (gtk_widget_get_realized (GTK_WIDGET (sidebar->tree_view))) {
		gtk_tree_view_scroll_to_cell (sidebar->tree_view, sort_path, NULL, FALSE, 0, 0);
	}
	gtk_tree_path_free (sort_path);

	nemo_file_unref (file);
	refresh_highlight (sidebar);

	return FALSE;
}

static void
schedule_show_selection (NemoPlacesSidebar *sidebar)
{
	if (sidebar->show_selection_idle_id == 0) {
		sidebar->show_selection_idle_id = g_idle_add (show_selection_idle_callback, sidebar);
	}
}

static void
schedule_select_and_show_location (NemoPlacesSidebar *sidebar, char *location)
{
	if (sidebar->selection_location != NULL) {
		g_free (sidebar->selection_location);
	}
	sidebar->selection_location = g_strdup (location);
	schedule_show_selection (sidebar);
}


static void
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
		       GVolume *volume,
		       NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
			     GDrive         *drive,
			     NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
			  GDrive         *drive,
			  NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
			GDrive         *drive,
			NemoPlacesSidebar *sidebar)
{
	update_places_on_idle (sidebar);
}

static gboolean
over_eject_button (NemoPlacesSidebar *sidebar,
		   gint x,
		   gint y,
		   GtkTreePath **path)
{
    GtkTreeViewColumn *column;
    int width, col_x_offset, cell_x_offset;
    gboolean show_eject;
    GtkTreeIter iter;
    GtkTreeModel *model;

    *path = NULL;
    model = gtk_tree_view_get_model (sidebar->tree_view);

    if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
                                       x, y,
                                       path, &column, &col_x_offset, NULL)) {

        gtk_tree_model_get_iter (model, &iter, *path);
        gtk_tree_model_get (model, &iter,
                            PLACES_SIDEBAR_COLUMN_EJECT, &show_eject,
                            -1);

        if (!show_eject) {
            goto out;
        }

        if (column == sidebar->eject_column) {
            gtk_tree_view_column_cell_set_cell_data (column, model, &iter, FALSE, FALSE);

            gtk_tree_view_column_cell_get_position (column,
                                                    sidebar->eject_icon_cell_renderer,
                                                    &cell_x_offset, &width);
            if ((col_x_offset >= cell_x_offset) && (col_x_offset < cell_x_offset + width)) {
                return TRUE;
            }
        }
    }

    out:
    if (*path != NULL) {
        gtk_tree_path_free (*path);
        *path = NULL;
    }

    return FALSE;
}

static gboolean
clicked_eject_button (NemoPlacesSidebar *sidebar,
		      GtkTreePath **path)
{
	GdkEvent *event;

	event = gtk_get_current_event ();

	if (event) {
		GdkEventButton *button_event = (GdkEventButton *) event;
		if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
		    over_eject_button (sidebar, button_event->x, button_event->y, path)) {
			return TRUE;
		}
	}

	gdk_event_free(event);
	return FALSE;
}

static void
desktop_setting_changed_callback (gpointer user_data)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (user_data);

	update_places (sidebar);
}




static void
loading_uri_callback (NemoWindow *window,
                      char *location,
                      NemoPlacesSidebar *sidebar)
{
    GtkTreeSelection *selection;
    GtkTreeIter       iter_cat, iter_child;
    gboolean          valid_cat, valid_child;
    char              *uri;
    gboolean found = FALSE;
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);

    if(sidebar->use_file_treeview) {
		schedule_select_and_show_location (sidebar, location);
    } else {
		if (strcmp (sidebar->selection_location, location) != 0) {
		    g_free (sidebar->selection_location);
		            sidebar->selection_location = g_strdup (location);

		    /* set selection if any place matches location */
		    selection = gtk_tree_view_get_selection (sidebar->tree_view);
		    gtk_tree_selection_unselect_all (selection);
		    valid_cat = gtk_tree_model_get_iter_first (model,
		                                                &iter_cat);

		    while (valid_cat) {
		        valid_child = gtk_tree_model_iter_children (model,
		                                                    &iter_child,
		                                                    &iter_cat);
		        while (valid_child) {
		            gtk_tree_model_get (model, &iter_child,
		                           	    PLACES_SIDEBAR_COLUMN_URI, &uri,
		                                -1);
		            if (uri != NULL) {
		                if (strcmp (uri, location) == 0) {
		                    g_free (uri);
		                    gtk_tree_selection_select_iter (selection, &iter_child);
		                    found = TRUE;
		                    break;
		                }
		                g_free (uri);
		            }
		            valid_child = gtk_tree_model_iter_next (model,
		                                                    &iter_child);
		        }
		        if (found) {
		            break;
		        }
		        valid_cat = gtk_tree_model_iter_next (model,
									                     &iter_cat);
			}
		}
	}
}



typedef struct {
    NemoPlacesSidebar *sidebar;
    GdkRectangle rect;
    SectionType section_type;
} CategoryExpandPayload;

static gboolean
pointer_is_still_in_cell (gint x,
                          gint y,
                  GdkRectangle rect)
{
    gint max_x = rect.x + rect.width;
    gint max_y = rect.y + rect.height;
    if ((x >= rect.x && x <= max_x) &&
        (y >= rect.y && y <= max_y)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
maybe_expand_category (gpointer data)
{
    CategoryExpandPayload *payload = (CategoryExpandPayload *) data;
    GdkDeviceManager *manager;
    GdkDevice *pointer;
    GdkWindow *window;
    int x, y;
    g_assert (GTK_IS_WIDGET (payload->sidebar->tree_view));
    window = gtk_widget_get_window (GTK_WIDGET (payload->sidebar->tree_view));

    manager = gdk_display_get_device_manager (gtk_widget_get_display (GTK_WIDGET (payload->sidebar->tree_view)));
    pointer = gdk_device_manager_get_client_pointer (manager);
    gdk_window_get_device_position (window, pointer,
                                    &x, &y, NULL);
    if (pointer_is_still_in_cell (x, y, payload->rect)) {
        expand_or_collapse_category (payload->sidebar, payload->section_type, TRUE);
    }

    g_source_remove (payload->sidebar->expand_timeout_source);
    payload->sidebar->expand_timeout_source = 0;
    return FALSE;
}


static gboolean
cat_is_expanded (NemoPlacesSidebar *sidebar,
                 SectionType section_type)
{
    switch (section_type) {
        case SECTION_COMPUTER:
            return sidebar->my_computer_expanded;
        case SECTION_BOOKMARKS:
            return sidebar->bookmarks_expanded;
        case SECTION_DEVICES:
            return sidebar->devices_expanded;
        case SECTION_NETWORK:
            return sidebar->network_expanded;
        case SECTION_XDG_BOOKMARKS:
        default:
            return TRUE;
    }
}


static GtkTreeViewDropPosition
get_drag_type (NemoPlacesSidebar *sidebar,
                           gchar *drop_target_uri,
                     GdkRectangle rect,
                              int y,
                      SectionType section_type)
{
    gint zone;
    gint upper_bound = rect.y + (rect.height / 4);
    gint lower_bound = rect.y + rect.height - (rect.height / 4);

    if (y <= upper_bound) {
        zone = POSITION_UPPER;
    } else if (y > upper_bound && y < lower_bound) {
        return GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
    } else {
        zone = POSITION_LOWER;
    }

    if (g_strcmp0 (drop_target_uri, sidebar->top_bookend_uri) == 0 &&
        zone == POSITION_LOWER) {
        return GTK_TREE_VIEW_DROP_AFTER;
    } else  if (g_strcmp0 (drop_target_uri, sidebar->bottom_bookend_uri) == 0 &&
        zone == POSITION_UPPER) {
        return GTK_TREE_VIEW_DROP_BEFORE;
    }

    if ((section_type == SECTION_XDG_BOOKMARKS || section_type == SECTION_BOOKMARKS)
        && zone == POSITION_UPPER) {
        return GTK_TREE_VIEW_DROP_BEFORE;
    } else if ((section_type == SECTION_XDG_BOOKMARKS || section_type == SECTION_BOOKMARKS)
        && zone == POSITION_LOWER) {
        return GTK_TREE_VIEW_DROP_AFTER;
    } else {
        /* or else you want to drag items INTO the existing bookmarks */
        return GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
    }
}


/* Computes the appropriate row and position for dropping */
static gboolean
compute_drop_position (GtkTreeView *tree_view,
		       int                      x,
		       int                      y,
		       GtkTreePath            **path,
		       GtkTreeViewDropPosition *pos,
		       NemoPlacesSidebar *sidebar)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	PlaceType place_type;
	SectionType section_type;
    gchar *drop_target_uri = NULL;

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view,
						x, y,
						path, pos)) {
		return FALSE;
	}
	model = gtk_tree_view_get_model (tree_view);

	gtk_tree_model_get_iter (model, &iter, *path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                PLACES_SIDEBAR_COLUMN_URI, &drop_target_uri,
			    -1);
	if(drop_target_uri==NULL) {
		drop_target_uri = get_fm_tree_model_uri (sidebar, model, &iter);
	}
	if (!cat_is_expanded (sidebar, section_type) && place_type == PLACES_HEADING) {
        if (sidebar->expand_timeout_source > 0) {
            goto fail;
        }
        CategoryExpandPayload *payload;
        GtkTreeViewColumn *col;
        col = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0);
        payload = g_new0 (CategoryExpandPayload, 1);
        payload->sidebar = sidebar;
        gtk_tree_view_get_cell_area (tree_view,
                                     *path,
                                     col,
                                     &payload->rect);
        payload->section_type = section_type;
        sidebar->expand_timeout_source = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                                             DRAG_EXPAND_CATEGORY_DELAY,
                                                             (GSourceFunc) maybe_expand_category,
                                                             payload,
                                                             (GDestroyNotify) g_free);
        goto fail;
	} else if (place_type == PLACES_HEADING) {
        if (section_type == SECTION_BOOKMARKS &&
            (int)nemo_bookmark_list_length (sidebar->bookmarks) == sidebar->bookmark_breakpoint) {
            *pos = GTK_TREE_VIEW_DROP_AFTER;
            g_free (drop_target_uri);
            return TRUE;
        } else {
            goto fail;
        }
    }

	if (section_type != SECTION_XDG_BOOKMARKS &&
        section_type != SECTION_BOOKMARKS &&
	    sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW &&
        g_strcmp0 (drop_target_uri, sidebar->top_bookend_uri) != 0) {
		/* don't allow dropping bookmarks into non-bookmark areas */

        goto fail;
	}

    if (g_strcmp0 (drop_target_uri, "recent:///") == 0) {
        goto fail;
    }

    GdkRectangle rect;
    GtkTreeViewColumn *col;
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0);
    gtk_tree_view_get_cell_area (tree_view,
                                     *path,
                                     col,
                                     &rect);

    *pos = get_drag_type (sidebar, drop_target_uri, rect, y, section_type);

	if (*pos != GTK_TREE_VIEW_DROP_BEFORE &&
	    sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
		/* bookmark rows are never dragged into other bookmark rows */
		*pos = GTK_TREE_VIEW_DROP_AFTER;
	}

	return TRUE;

fail:
    g_free (drop_target_uri);
    gtk_tree_path_free (*path);
    *path = NULL;
    return FALSE;
}

static gboolean
get_drag_data (GtkTreeView *tree_view,
	       GdkDragContext *context,
	       unsigned int time)
{
	GdkAtom target;

	target = gtk_drag_dest_find_target (GTK_WIDGET (tree_view),
					    context,
					    NULL);

	if (target == GDK_NONE) {
		return FALSE;
	}

	gtk_drag_get_data (GTK_WIDGET (tree_view),
			   context, target, time);

	return TRUE;
}

static void
free_drag_data (NemoPlacesSidebar *sidebar)
{
	sidebar->drag_data_received = FALSE;

	if (sidebar->drag_list) {
		nemo_drag_destroy_selection_list (sidebar->drag_list);
		sidebar->drag_list = NULL;
	}

    g_clear_pointer (&sidebar->desktop_dnd_source_fs, g_free);
    sidebar->desktop_dnd_can_delete_source = FALSE;
}

static gboolean
can_accept_file_as_bookmark (NemoFile *file)
{
	return (nemo_file_is_directory (file) &&
		!is_built_in_bookmark (file));
}

static gboolean
can_accept_items_as_bookmarks (const GList *items)
{
	int max;
	char *uri;
	NemoFile *file;

	/* Iterate through selection checking if item will get accepted as a bookmark.
	 * If more than 100 items selected, return an over-optimistic result.
	 */
	for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
		uri = ((NemoDragSelectionItem *)items->data)->uri;
		file = nemo_file_get_by_uri (uri);
		if (!can_accept_file_as_bookmark (file)) {
			nemo_file_unref (file);
			return FALSE;
		}
		nemo_file_unref (file);
	}

	return TRUE;
}

static gboolean
drag_motion_callback (GtkTreeView *tree_view,
		      GdkDragContext *context,
		      int x,
		      int y,
		      unsigned int time,
		      NemoPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeViewDropPosition pos;
	int action;
	GtkTreeIter iter;
	char *uri;
	gboolean res;

    action = 0;

	if (!sidebar->drag_data_received) {
		if (!get_drag_data (tree_view, context, time)) {
			return FALSE;
		}
	}

    if (!sidebar->in_drag) {
        sidebar->in_drag = TRUE;
		if(!sidebar->use_file_treeview) {
        		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->store_filter));
		}
    }

	path = NULL;
	res = compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

	if (!res) {
        gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
		goto out;
	}

	if (pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    pos == GTK_TREE_VIEW_DROP_AFTER ) {
		if (sidebar->drag_data_received &&
		    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
			action = GDK_ACTION_MOVE;
		} else if (can_accept_items_as_bookmarks (sidebar->drag_list)) {
			action = GDK_ACTION_COPY;
		} else {
			action = 0;
		}
	} else {
		if (sidebar->drag_list == NULL) {
			action = 0;
		} else {
			GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
			gtk_tree_model_get_iter (GTK_TREE_MODEL (model),
						 &iter, path);
			gtk_tree_model_get (model,
					    &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
			if(uri == NULL) {
				uri = get_fm_tree_model_uri (sidebar, model, &iter);
			}
            nemo_drag_default_drop_action_for_icons (context,
                                                     uri,
                                                     sidebar->drag_list,
                                                     &action,
                                                     &sidebar->desktop_dnd_source_fs,
                                                     &sidebar->desktop_dnd_can_delete_source);
            g_free (uri);
		}
	}

	if (action != 0) {
		gtk_tree_view_set_drag_dest_row (tree_view, path, pos);
	}

	if (path != NULL) {
		gtk_tree_path_free (path);
	}

 out:
	g_signal_stop_emission_by_name (tree_view, "drag-motion");

	if (action != 0) {
		gdk_drag_status (context, action, time);
	} else {
		gdk_drag_status (context, 0, time);
	}

	return TRUE;
}

static void
drag_leave_callback (GtkTreeView *tree_view,
		     GdkDragContext *context,
		     unsigned int time,
		     NemoPlacesSidebar *sidebar)
{
	free_drag_data (sidebar);
	gtk_tree_view_set_drag_dest_row (tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
	g_signal_stop_emission_by_name (tree_view, "drag-leave");
}

/* Parses a "text/uri-list" string and inserts its URIs as bookmarks */
static void
bookmarks_drop_uris (NemoPlacesSidebar *sidebar,
                     GtkSelectionData  *selection_data,
                                  int   position,
                          SectionType   section_type)
{
	NemoBookmark *bookmark;
	NemoFile *file;
	char *uri;
	char **uris;
	int i;
	GFile *location;

	uris = gtk_selection_data_get_uris (selection_data);
	if (!uris)
		return;

	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		file = nemo_file_get_by_uri (uri);

		if (!can_accept_file_as_bookmark (file)) {
			nemo_file_unref (file);
			continue;
		}

		uri = nemo_file_get_drop_target_uri (file);
		location = g_file_new_for_uri (uri);
		nemo_file_unref (file);

		bookmark = nemo_bookmark_new (location, NULL, NULL, NULL);

		if (!nemo_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
            if (position < sidebar->bookmark_breakpoint ||
                (position == sidebar->bookmark_breakpoint && (section_type == SECTION_XDG_BOOKMARKS ||
                                                              section_type == SECTION_COMPUTER))) {
                increment_bookmark_breakpoint (sidebar);
            }
			nemo_bookmark_list_insert_item (sidebar->bookmarks, bookmark, position++);
		}

		g_object_unref (location);
		g_object_unref (bookmark);
		g_free (uri);
	}

	g_strfreev (uris);
}

static GList *
uri_list_from_selection (GList *selection)
{
	NemoDragSelectionItem *item;
	GList *ret;
	GList *l;

	ret = NULL;
	for (l = selection; l != NULL; l = l->next) {
		item = l->data;
		ret = g_list_prepend (ret, item->uri);
	}

	return g_list_reverse (ret);
}

static GList*
build_selection_list (const char *data)
{
	NemoDragSelectionItem *item;
	GList *result;
	char **uris;
	char *uri;
	int i;

	uris = g_uri_list_extract_uris (data);

	result = NULL;
	for (i = 0; uris[i]; i++) {
		uri = uris[i];
		item = nemo_drag_selection_item_new ();
		item->uri = g_strdup (uri);
		item->got_icon_position = FALSE;
		result = g_list_prepend (result, item);
	}

	g_strfreev (uris);

	return g_list_reverse (result);
}

static gboolean
get_selected_iter (NemoPlacesSidebar *sidebar,
		   GtkTreeIter *iter)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);

	return gtk_tree_selection_get_selected (selection, NULL, iter);
}

static gboolean
get_selected_iter_for_store (NemoPlacesSidebar *sidebar,
                   GtkTreeIter       *fm_iter)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter view_iter;

    selection = gtk_tree_view_get_selection (sidebar->tree_view);

    if (!gtk_tree_selection_get_selected (selection, &model, &view_iter))
        return FALSE;

    get_fm_tree_model_iter (model, &view_iter, fm_iter);

    return TRUE;
}

static void
update_bookmark_breakpoint (NemoPlacesSidebar *sidebar,
                                  SectionType  old_type,
                                  SectionType  new_type)
{
    if (old_type != new_type) {
        if (old_type == SECTION_XDG_BOOKMARKS && new_type != SECTION_COMPUTER)
            decrement_bookmark_breakpoint (sidebar);
        else if (old_type == SECTION_BOOKMARKS)
            increment_bookmark_breakpoint (sidebar);
    }
}

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (NemoPlacesSidebar *sidebar,
                                 int  new_position,
                         SectionType  new_section_type)
{
	GtkTreeIter iter;
	PlaceType type;
    SectionType old_section_type;
	int old_position;

	/* Get the selected path */
	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &old_section_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, &old_position,
			    -1);

	if (type != PLACES_BOOKMARK ||
	    old_position < 0 ||
	    old_position >= (int)nemo_bookmark_list_length (sidebar->bookmarks)) {
		return;
	}

    update_bookmark_breakpoint (sidebar, old_section_type, new_section_type);

	nemo_bookmark_list_move_item (sidebar->bookmarks, old_position,
					  new_position);

    if (old_position == new_position)
        update_places (sidebar);
}

static gboolean
idle_hide_bookmarks (gpointer user_data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    if (sidebar->in_drag) {
        sidebar->in_drag = FALSE;
        if(!sidebar->use_file_treeview) {
           gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (sidebar->store_filter));
        }
    }

    return FALSE;
}

static void
drag_data_received_callback (GtkWidget *widget,
			     GdkDragContext *context,
			     int x,
			     int y,
			     GtkSelectionData *selection_data,
			     unsigned int info,
			     unsigned int time,
			     NemoPlacesSidebar *sidebar)
{
	GtkTreeView *tree_view;
	GtkTreePath *tree_path;
	GtkTreeViewDropPosition tree_pos;
	GtkTreeIter iter;
	int position;
	GtkTreeModel *model;
	char *drop_uri;
	GList *selection_list, *uris;
	PlaceType place_type;
	SectionType section_type;
	gboolean success;

	tree_view = GTK_TREE_VIEW (widget);

	if (!sidebar->drag_data_received) {
		if (gtk_selection_data_get_target (selection_data) != GDK_NONE &&
		    info == TEXT_URI_LIST) {
			sidebar->drag_list = build_selection_list ((const gchar *) gtk_selection_data_get_data (selection_data));
		} else {
			sidebar->drag_list = NULL;
		}
		sidebar->drag_data_received = TRUE;
		sidebar->drag_data_info = info;
	}

	g_signal_stop_emission_by_name (widget, "drag-data-received");

	if (!sidebar->drop_occured) {
		return;
	}

	/* Compute position */
	success = compute_drop_position (tree_view, x, y, &tree_path, &tree_pos, sidebar);
	if (!success) {
		goto out;
	}

	success = FALSE;

	if (tree_pos == GTK_TREE_VIEW_DROP_BEFORE ||
	    tree_pos == GTK_TREE_VIEW_DROP_AFTER) {
		model = gtk_tree_view_get_model (tree_view);
		if (!gtk_tree_model_get_iter (model, &iter, tree_path)) {
			goto out;
		}

		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
    				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    PLACES_SIDEBAR_COLUMN_INDEX, &position,
				    -1);

        if (section_type == SECTION_COMPUTER && tree_pos == GTK_TREE_VIEW_DROP_BEFORE) {
            position = nemo_bookmark_list_length(sidebar->bookmarks);
        } else if (section_type == SECTION_BOOKMARKS && position == -1) {
            /* Check for (temporarily) visible Bookmarks heading, only drop-reactive when
             * it has no children, so we can assume that (bookmark_breakpoint == bookmark_count)
             */
            position = nemo_bookmark_list_length (sidebar->bookmarks);
        } else if (tree_pos == GTK_TREE_VIEW_DROP_AFTER &&
                   (section_type == SECTION_XDG_BOOKMARKS || section_type == SECTION_BOOKMARKS)) {
            position++;
        }

		switch (info) {
		case TEXT_URI_LIST:
			bookmarks_drop_uris (sidebar, selection_data, position, section_type);
			success = TRUE;
			break;
		case GTK_TREE_MODEL_ROW:
			reorder_bookmarks (sidebar, position, section_type);
			success = TRUE;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	} else {
		GdkDragAction real_action;

		/* file transfer requested */
		real_action = gdk_drag_context_get_selected_action (context);

		if (real_action == GDK_ACTION_ASK) {
			real_action =
				nemo_drag_drop_action_ask (GTK_WIDGET (tree_view),
							       gdk_drag_context_get_actions (context));
		}

		if (real_action > 0) {
			model = gtk_tree_view_get_model (tree_view);

			gtk_tree_model_get_iter (model, &iter, tree_path);
			gtk_tree_model_get (model, &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &drop_uri,
					    -1);
			if(drop_uri==NULL) {
				drop_uri = get_fm_tree_model_uri (sidebar, model, &iter);
			}
			switch (info) {
			case TEXT_URI_LIST:
				selection_list = build_selection_list ((const gchar *) gtk_selection_data_get_data (selection_data));
				uris = uri_list_from_selection (selection_list);

                GList *l;

                if (g_strcmp0 (drop_uri, "favorites:///") == 0) {
                    for (l = uris; l != NULL; l = l->next) {
                        gchar *uri = (gchar *) l->data;
                        NemoFile *source_file = nemo_file_get_by_uri (uri);
                        nemo_file_set_is_favorite (source_file, TRUE);
                        nemo_file_unref (source_file);
                    }
                } else {
                    nemo_file_operations_copy_move (uris, NULL, drop_uri,
                                        real_action, GTK_WIDGET (tree_view),
                                        NULL, NULL);
                }

				nemo_drag_destroy_selection_list (selection_list);
				g_list_free (uris);
				success = TRUE;
				break;
			case GTK_TREE_MODEL_ROW:
				success = FALSE;
				break;
			default:
				g_assert_not_reached ();
				break;
			}

			g_free (drop_uri);
		}
	}

out:
	sidebar->drop_occured = FALSE;
	free_drag_data (sidebar);
	gtk_drag_finish (context, success, FALSE, time);
	gtk_tree_path_free (tree_path);

    g_timeout_add (250, (GSourceFunc) idle_hide_bookmarks, sidebar);
}

static gboolean
drag_drop_callback (GtkTreeView *tree_view,
		    GdkDragContext *context,
		    int x,
		    int y,
		    unsigned int time,
		    NemoPlacesSidebar *sidebar)
{
	gboolean retval = FALSE;
	sidebar->drop_occured = TRUE;
	retval = get_drag_data (tree_view, context, time);
	g_signal_stop_emission_by_name (tree_view, "drag-drop");
	return retval;
}

static void
check_unmount_and_eject (GMount *mount,
			 GVolume *volume,
			 GDrive *drive,
			 gboolean *show_unmount,
			 gboolean *show_eject)
{
	*show_unmount = FALSE;
	*show_eject = FALSE;

	if (drive != NULL) {
		*show_eject = g_drive_can_eject (drive);
	}

	if (volume != NULL) {
		*show_eject |= g_volume_can_eject (volume);
	}
	if (mount != NULL) {
		*show_eject |= g_mount_can_eject (mount);
		*show_unmount = g_mount_can_unmount (mount) && !*show_eject;
	}
}

static void
check_visibility (GMount           *mount,
		  GVolume          *volume,
		  GDrive           *drive,
		  gboolean         *show_mount,
		  gboolean         *show_unmount,
		  gboolean         *show_eject,
		  gboolean         *show_rescan,
		  gboolean         *show_start,
		  gboolean         *show_stop)
{
	*show_mount = FALSE;
	*show_rescan = FALSE;
	*show_start = FALSE;
	*show_stop = FALSE;

	check_unmount_and_eject (mount, volume, drive, show_unmount, show_eject);

	if (drive != NULL) {
		if (g_drive_is_removable (drive) &&
		    !g_drive_is_media_check_automatic (drive) &&
		    g_drive_can_poll_for_media (drive))
			*show_rescan = TRUE;

		*show_start = g_drive_can_start (drive) || g_drive_can_start_degraded (drive);
		*show_stop  = g_drive_can_stop (drive);

		if (*show_stop)
			*show_unmount = FALSE;
	}

	if (volume != NULL) {
		if (mount == NULL)
			*show_mount = g_volume_can_mount (volume);
	}
}

static void
set_action_sensitive (GtkActionGroup *action_group,
                      const gchar    *name,
                      gboolean        sensitive)
{
    GtkAction *action;

    action = gtk_action_group_get_action (action_group, name);
    gtk_action_set_sensitive (action, sensitive);
}

static void
set_action_visible (GtkActionGroup *action_group,
                    const gchar    *name,
                    gboolean        visible)
{
    GtkAction *action;

    action = gtk_action_group_get_action (action_group, name);
    gtk_action_set_visible (action, visible);
}

static GdkAtom copied_files_atom;

static void
clipboard_contents_received_callback (GtkClipboard     *clipboard,
                      GtkSelectionData *selection_data,
                      gpointer          data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (data);

    if (gtk_selection_data_get_data_type (selection_data) == copied_files_atom
        && gtk_selection_data_get_length (selection_data) > 0) {
        set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_PASTE, TRUE);
    }
}


static gboolean
is_parent_writable (NemoFile *file)
{
	NemoFile *parent;
	gboolean result;

	parent = nemo_file_get_parent (file);

	/* No parent directory, return FALSE */
	if (parent == NULL) {
		return FALSE;
	}

	result = nemo_file_can_write (parent);
	nemo_file_unref (parent);

	return result;
}

static GtkWindow *
fm_tree_view_get_containing_window (NemoPlacesSidebar *sidebar)
{
	GtkWidget *window;

	g_assert (NEMO_PLACES_SIDEBAR (sidebar));

	window = gtk_widget_get_ancestor (GTK_WIDGET (sidebar), GTK_TYPE_WINDOW);
	if (window == NULL) {
		return NULL;
	}

	return GTK_WINDOW (window);
}

static void
fm_tree_view_delete_cb (GtkAction *action,
		        NemoPlacesSidebar *sidebar)
{
	GList *location_list;

	if (!g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE)) {
		return;
	}

	location_list = g_list_prepend (NULL,
					nemo_file_get_location (sidebar->popup_file));

	nemo_file_operations_delete (location_list, fm_tree_view_get_containing_window (sidebar), NULL, NULL);
	g_list_free_full (location_list, g_object_unref);
}

static void
fm_tree_view_trash_cb (GtkAction *action,
		       NemoPlacesSidebar *sidebar)
{
	GList *list;

	if (!nemo_file_can_trash (sidebar->popup_file)) {
		return;
	}

	list = g_list_prepend (NULL,
			       nemo_file_get_location (sidebar->popup_file));

	nemo_file_operations_trash_or_delete (list,
						  fm_tree_view_get_containing_window (sidebar),
						  NULL, NULL);
	g_list_free_full (list, g_object_unref);
}

static void
fm_tree_view_pin_unpin_cb (GtkAction *action,
                           NemoPlacesSidebar *sidebar)
{
    nemo_file_set_pinning (sidebar->popup_file,
                           !nemo_file_get_pinning (sidebar->popup_file));
}

static void
paste_clipboard_data (NemoPlacesSidebar *sidebar,
		      GtkSelectionData *selection_data,
		      char *destination_uri)
{
	gboolean cut;
	GList *item_uris;

	cut = FALSE;
	item_uris = nemo_clipboard_get_uri_list_from_selection_data (selection_data, &cut,
									 copied_files_atom);

	if (item_uris == NULL|| destination_uri == NULL) {
		nemo_window_push_status (sidebar->window,
						  _("There is nothing on the clipboard to paste."));
	} else {
		nemo_file_operations_copy_move
			(item_uris, NULL, destination_uri,
			 cut ? GDK_ACTION_MOVE : GDK_ACTION_COPY,
			 GTK_WIDGET (sidebar->tree_view),
			 NULL, NULL);

		/* If items are cut then remove from clipboard */
		if (cut) {
			gtk_clipboard_clear (nemo_clipboard_get (GTK_WIDGET (sidebar)));
		}

		g_list_free_full (item_uris, g_free);
	}
}

static void
paste_into_clipboard_received_callback (GtkClipboard     *clipboard,
					GtkSelectionData *selection_data,
					gpointer          data)
{
	char *directory_uri;
	NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (data);

	directory_uri = nemo_file_get_uri (sidebar->popup_file);

	paste_clipboard_data (sidebar, selection_data, directory_uri);

	g_free (directory_uri);
}

static void
fm_tree_view_paste_cb (GtkAction *action,
		       NemoPlacesSidebar *sidebar)
{
	gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (sidebar->tree_view)),
					copied_files_atom,
					paste_into_clipboard_received_callback, sidebar);
}

static void
copy_or_cut_files (NemoPlacesSidebar *sidebar,
		   gboolean cut)
{
    GtkClipboard *clipboard;
	char *status_string, *name;
	NemoClipboardInfo info;
        GtkTargetList *target_list;
        GtkTargetEntry *targets;
        int n_targets;

	info.cut = cut;
	info.files = g_list_prepend (NULL, sidebar->popup_file);

        target_list = gtk_target_list_new (NULL, 0);
        gtk_target_list_add (target_list, copied_files_atom, 0, 0);
        gtk_target_list_add_uri_targets (target_list, 0);
        gtk_target_list_add_text_targets (target_list, 0);

        targets = gtk_target_table_new_from_list (target_list, &n_targets);
        gtk_target_list_unref (target_list);

    clipboard = nemo_clipboard_get (GTK_WIDGET (sidebar));

    gtk_clipboard_set_with_data (clipboard,
                                 targets, n_targets,
                                 nemo_get_clipboard_callback, nemo_clear_clipboard_callback,
                                 NULL);
    gtk_clipboard_set_can_store (clipboard, NULL, 0);
    gtk_target_table_free (targets, n_targets);

	nemo_clipboard_monitor_set_clipboard_info (nemo_clipboard_monitor_get (),
	                                               &info);
	g_list_free (info.files);

	name = nemo_file_get_display_name (sidebar->popup_file);
	if (cut) {
		status_string = g_strdup_printf (_("\"%s\" will be moved "
						   "if you select the Paste command"),
						 name);
	} else {
		status_string = g_strdup_printf (_("\"%s\" will be copied "
						   "if you select the Paste command"),
						 name);
	}
	g_free (name);

	nemo_window_push_status (sidebar->window,
					  status_string);
	g_free (status_string);
}

static void
fm_tree_view_cut_cb (GtkAction *action,
		     NemoPlacesSidebar *sidebar)
{
	copy_or_cut_files (sidebar, TRUE);
}

static void
fm_tree_view_copy_cb (GtkAction *action,
		      NemoPlacesSidebar *sidebar)
{
	copy_or_cut_files (sidebar, FALSE);
}


static void
update_menu_states_tree (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
    gboolean parent_file_is_writable;
    gboolean file_is_home_or_desktop;
    gboolean file_is_special_link;
    gboolean can_move_file_to_trash;
    gboolean can_delete_file;
    gboolean using_browser;
    gboolean is_dir;
    gboolean can_write;
	NemoFile *file=NULL;

	if (get_selected_iter_for_store (sidebar, &iter)) {
		file = fm_tree_model_iter_get_file (FM_TREE_MODEL (sidebar->store), &iter);
	}
    using_browser = g_settings_get_boolean (nemo_preferences,
                                            NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

    gboolean show_unmount = FALSE;
    gboolean show_eject = FALSE;
    GMount *mount = NULL;

    NemoFile *parent = nemo_file_get_parent (file);
    GList *selected = g_list_prepend (NULL, file);

    nemo_action_manager_update_action_states (sidebar->action_manager,
                                              sidebar->action_action_group,
                                              selected,
                                              parent,
                                              FALSE,
                                              FALSE,
                                              GTK_WINDOW (sidebar->window));
	nemo_file_ref (file);
    nemo_file_list_free (selected);   // fils is unrefed here
    nemo_file_unref (parent);

    is_dir = nemo_file_is_directory (file);
    can_write = nemo_file_can_write (file);

    set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_OPEN_ALTERNATE, is_dir);
    set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_OPEN_IN_NEW_TAB, is_dir);
    set_action_visible (sidebar->tv_action_group, NEMO_ACTION_OPEN_ALTERNATE, using_browser);
    set_action_visible (sidebar->tv_action_group, NEMO_ACTION_OPEN_IN_NEW_TAB, using_browser);
    set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_NEW_FOLDER, is_dir && can_write);
    set_action_visible (sidebar->tv_action_group, NEMO_ACTION_NEW_FOLDER, !nemo_file_is_in_favorites (file));
    set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_PASTE, FALSE);

    if (is_dir && can_write) {
        gtk_clipboard_request_contents (nemo_clipboard_get (GTK_WIDGET (sidebar->tree_view)),
                                        copied_files_atom,
                                        clipboard_contents_received_callback, sidebar);
    }

    can_move_file_to_trash = nemo_file_can_trash (file);
    set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_TRASH, can_move_file_to_trash);

    if (g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_ENABLE_DELETE)) {
    	parent_file_is_writable = is_parent_writable (file);
    	file_is_home_or_desktop = nemo_file_is_home (file)
    		|| nemo_file_is_desktop_directory (file);
    	file_is_special_link = NEMO_IS_DESKTOP_ICON_FILE (file);

    	can_delete_file = parent_file_is_writable
    		&& !file_is_home_or_desktop
    		&& !file_is_special_link;

        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_DELETE, TRUE);
        set_action_sensitive (sidebar->tv_action_group, NEMO_ACTION_DELETE, can_delete_file);
    } else {
        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_DELETE, FALSE);
    }

    mount = fm_tree_model_get_mount_for_root_node_file (sidebar->store, file);
    if (mount) {
    	show_unmount = g_mount_can_unmount (mount);
    	show_eject = g_mount_can_eject (mount);
    }

    set_action_visible (sidebar->tv_action_group, NEMO_ACTION_UNMOUNT_VOLUME, show_unmount);
    set_action_visible (sidebar->tv_action_group, NEMO_ACTION_EJECT_VOLUME, show_eject);

    if (!nemo_file_is_in_favorites (file)) {
        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PIN_FILE,  !nemo_file_get_pinning (file));
        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_UNPIN_FILE, nemo_file_get_pinning (file));
    } else {
        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PIN_FILE, FALSE);
        set_action_visible (sidebar->tv_action_group, NEMO_ACTION_UNPIN_FILE, FALSE);
    }
	set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PROPERTIES, FALSE);
	nemo_file_unref (file);
}

static void
update_menu_states (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type;
	GDrive *drive = NULL;
	GVolume *volume = NULL;
	GMount *mount = NULL;
	GFile *location;
	NemoDirectory *directory = NULL;
	gboolean show_mount;
	gboolean show_unmount;
	gboolean show_eject;
	gboolean show_rescan;
	gboolean show_start;
	gboolean show_stop;
	gboolean show_empty_trash;
	gboolean show_properties;
	char *uri = NULL;

	type = PLACES_BUILT_IN;

	if (sidebar->popup_menu == NULL) {
		return;
	}

	if (get_selected_iter_for_store (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
 				    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    -1);
	}
	if(uri==NULL && sidebar->use_file_treeview) {
		uri = get_fm_tree_model_uri (sidebar, sidebar->store, &iter);
	}
    if (uri) {
		NemoFile *file = sidebar->use_file_treeview ? fm_tree_model_iter_get_file(sidebar->store, &iter) : nemo_file_get_by_uri (uri);
		if(file) {
			directory = nemo_directory_get_for_file(file);
			show_properties = nemo_directory_is_local (directory);
		}
        NemoFile *parent = nemo_file_get_parent (file);

        GList *selection = g_list_prepend (NULL, file);

        nemo_action_manager_update_action_states (sidebar->action_manager,
                                                  sidebar->action_action_group,
                                                  selection,
                                                  parent,
                                                  TRUE,
                                                  FALSE,
                                                  GTK_WINDOW (sidebar->window));
        /* nemo_file_list_free (selection); internally unrefs file! */
		nemo_file_list_free (selection);
        nemo_file_unref (parent);
    }

	/* Only show properties for local mounts */
	//show_properties = (mount != NULL);
	if (mount != NULL) {
		location = g_mount_get_default_location (mount);
		directory = nemo_directory_get (location);

		show_properties = nemo_directory_is_local (directory);

		nemo_directory_unref (directory);
		g_object_unref (location);
	}

	switch(type) {
	case PLACES_TREE_FOLDER: {
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_CUT, TRUE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_COPY, TRUE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PASTE, TRUE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_TRASH, TRUE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_DELETE, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, FALSE);
		if(sidebar->use_file_treeview)update_menu_states_tree(sidebar);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, TRUE);
		/*
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_REMOVE, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_RENAME, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_NEW_FOLDER, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, show_properties);
		 * */
	    }
		return;
	case PLACES_BUILT_IN:
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_REMOVE, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_RENAME, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, show_properties);
		break;
	case PLACES_MOUNTED_VOLUME: {
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_REMOVE, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_RENAME, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, show_properties);
	    }
		break;
	case PLACES_BOOKMARK:
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_REMOVE, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_RENAME, TRUE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, show_properties);
		break;
	default:
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_ADD_BOOKMARK, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_REMOVE, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_RENAME, FALSE);
		set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, FALSE);
		break;
	}
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_NEW_FOLDER, FALSE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_DELETE, FALSE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_UNMOUNT_VOLUME, FALSE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_EJECT_VOLUME, FALSE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PIN_FILE,  FALSE);
		set_action_visible (sidebar->tv_action_group, NEMO_ACTION_UNPIN_FILE, FALSE);
	set_action_visible (sidebar->tv_action_group, NEMO_ACTION_TRASH, FALSE);
	set_action_visible (sidebar->tv_action_group, NEMO_ACTION_CUT, FALSE);
	set_action_visible (sidebar->tv_action_group, NEMO_ACTION_COPY, FALSE);
	set_action_visible (sidebar->tv_action_group, NEMO_ACTION_PASTE, FALSE);


    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_EMPTY_TRASH_CONDITIONAL, !nemo_trash_monitor_is_empty ());


	/* We actually want both eject and unmount since eject will unmount all volumes.
	 * TODO: hide unmount if the drive only has a single mountable volume
	 */
	show_empty_trash = (uri != NULL) &&
			   (!strcmp (uri, "trash:///"));

    g_free (uri);

	check_visibility (mount, volume, drive,
 			  &show_mount, &show_unmount, &show_eject, &show_rescan, &show_start, &show_stop);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_MOUNT_VOLUME, show_mount);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_UNMOUNT_VOLUME, show_unmount);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_EJECT_VOLUME, show_eject);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_SIDEBAR_DETECT_MEDIA, show_rescan);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_START_VOLUME, show_start);
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_STOP_VOLUME, show_stop);

    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_EMPTY_TRASH_CONDITIONAL, show_empty_trash);
	/*
    set_action_visible (sidebar->bookmark_action_group, NEMO_ACTION_PROPERTIES, show_properties);
	 */

	/* Adjust start/stop items to reflect the type of the drive */
    GtkAction *start_action, *stop_action;

    start_action = gtk_action_group_get_action (sidebar->bookmark_action_group, NEMO_ACTION_START_VOLUME);
    stop_action = gtk_action_group_get_action (sidebar->bookmark_action_group, NEMO_ACTION_STOP_VOLUME);

	gtk_action_set_label (start_action, _("_Start"));
	gtk_action_set_label (stop_action, _("_Stop"));
	if ((show_start || show_stop) && drive != NULL) {
		switch (g_drive_get_start_stop_type (drive)) {
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			/* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
			gtk_action_set_label (start_action, _("_Power On"));
			gtk_action_set_label (stop_action, _("_Safely Remove Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_action_set_label (start_action, _("_Connect Drive"));
			gtk_action_set_label (stop_action, _("_Disconnect Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_action_set_label (start_action, _("_Start Multi-disk Device"));
			gtk_action_set_label (stop_action, _("_Stop Multi-disk Device"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			/* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
			gtk_action_set_label (start_action, _("_Unlock Drive"));
			gtk_action_set_label (stop_action, _("_Lock Drive"));
			break;

		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			// Uses defaults set above
			break;
		}
	}

    g_clear_object (&drive);
    g_clear_object (&volume);
    g_clear_object (&mount);
}

/* Callback used when the selection in the shortcuts tree changes */

static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
				NemoPlacesSidebar *sidebar)
{
    update_menu_states (sidebar);
}


static void
volume_mounted_cb (GVolume *volume,
		   gboolean success,
		   GObject *user_data)
{
	GMount *mount;
	NemoPlacesSidebar *sidebar;
	GFile *location;

	sidebar = NEMO_PLACES_SIDEBAR (user_data);

	sidebar->mounting = FALSE;

	mount = g_volume_get_mount (volume);
	if (mount != NULL) {
		location = g_mount_get_default_location (mount);

		if (sidebar->go_to_after_mount_slot != NULL) {
			if ((sidebar->go_to_after_mount_flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
				nemo_window_slot_open_location (sidebar->go_to_after_mount_slot, location,
								    sidebar->go_to_after_mount_flags);
			} else {
				NemoWindow *new, *cur;

				cur = NEMO_WINDOW (sidebar->window);
				new = nemo_application_create_window (nemo_application_get_singleton (),
									  gtk_window_get_screen (GTK_WINDOW (cur)));
				nemo_window_go_to (new, location);
			}
		}

		g_object_unref (G_OBJECT (location));
		g_object_unref (G_OBJECT (mount));
	}

	if (sidebar->go_to_after_mount_slot) {
		g_object_remove_weak_pointer (G_OBJECT (sidebar->go_to_after_mount_slot),
					      (gpointer *) &sidebar->go_to_after_mount_slot);
		sidebar->go_to_after_mount_slot = NULL;
	}
}

static void
drive_start_from_bookmark_cb (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
	GError *error;
	char *primary;
	char *name;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to start %s"), name);
			g_free (name);
			eel_show_error_dialog (primary,
					       error->message,
					       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}
}

static void
open_selected_bookmark (NemoPlacesSidebar *sidebar,
			GtkTreeModel	      *model,
			GtkTreeIter	      *iter,
			NemoWindowOpenFlags	      flags)
{
	NemoWindowSlot *slot;
	GFile *location;
	char *uri;

	if (!iter) {
		return;
	}
	GtkTreeIter fm_iter;
	get_fm_tree_model_iter (model, iter, &fm_iter);
	gtk_tree_model_get (sidebar->store, &fm_iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);
	if (!uri) {
		uri = get_fm_tree_model_uri (sidebar, sidebar->store, &fm_iter);
	}
	if (uri != NULL) {
		DEBUG ("Activating bookmark %s", uri);

		location = g_file_new_for_uri (uri);
		/* Navigate to the clicked location */
		if ((flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
			slot = nemo_window_get_active_slot (sidebar->window);
			nemo_window_slot_open_location (slot, location, flags);
		} else {
			NemoWindow *cur, *new;

			cur = NEMO_WINDOW (sidebar->window);
			new = nemo_application_create_window (nemo_application_get_singleton (),
								  gtk_window_get_screen (GTK_WINDOW (cur)));
			nemo_window_go_to (new, location);
		}
		g_object_unref (location);
		g_free (uri);

	} else {
		GDrive *drive;
		GVolume *volume;
		NemoWindowSlot *slt;

		if(sidebar->use_file_treeview) {
			if( iter_is_valid ( FM_TREE_MODEL(sidebar->store),  &fm_iter)) {
			  gtk_tree_model_get ( GTK_TREE_MODEL (sidebar->store), &fm_iter,
						PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
						PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
						-1);
			} else {
				return;
			}
		} else {
			  gtk_tree_model_get ( GTK_TREE_MODEL (sidebar->store), &fm_iter,
						PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
						PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
						-1);
		}
		if (volume != NULL && !sidebar->mounting) {
			sidebar->mounting = TRUE;

			g_assert (sidebar->go_to_after_mount_slot == NULL);

			slt = nemo_window_get_active_slot (sidebar->window);
			sidebar->go_to_after_mount_slot = slt;
			g_object_add_weak_pointer (G_OBJECT (sidebar->go_to_after_mount_slot),
						   (gpointer *) &sidebar->go_to_after_mount_slot);

			sidebar->go_to_after_mount_flags = flags | NEMO_WINDOW_OPEN_FLAG_MOUNT;

			nemo_file_operations_mount_volume_full (NULL, volume,
								    volume_mounted_cb,
								    G_OBJECT (sidebar));
		} else if (volume == NULL && drive != NULL &&
			   (g_drive_can_start (drive) || g_drive_can_start_degraded (drive))) {
			GMountOperation *mount_op;

			mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
			g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_from_bookmark_cb, NULL);
			g_object_unref (mount_op);
		}

		if (drive != NULL)
			g_object_unref (drive);
		if (volume != NULL)
			g_object_unref (volume);
	}
}

static void
open_shortcut_from_menu (NemoPlacesSidebar *sidebar,
			 NemoWindowOpenFlags	       flags)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;

	model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

	if (path != NULL && gtk_tree_model_get_iter (model, &iter, path)) {
		open_selected_bookmark (sidebar, model, &iter, flags);
	}

	gtk_tree_path_free (path);
}

static void
open_shortcut_cb (GtkAction		*item,
		  NemoPlacesSidebar	*sidebar)
{
	open_shortcut_from_menu (sidebar, 0);
}

static void
open_shortcut_in_new_window_cb (GtkAction	      *item,
				NemoPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
open_shortcut_in_new_tab_cb (GtkAction	      *item,
				NemoPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

/* Add bookmark for the selected item */
static void
add_bookmark (NemoPlacesSidebar *sidebar)
{
	//GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	GFile *location;
	NemoBookmark *bookmark;

	//model = gtk_tree_view_get_model (sidebar->tree_view);

	if (get_selected_iter_for_store (sidebar, &iter)) {
		gtk_tree_model_get (sidebar->store, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);
	    if (uri == NULL) {
			uri = get_fm_tree_model_uri (sidebar, sidebar->store, &iter);
		}
		if (uri == NULL) {
			return;
		}

		location = g_file_new_for_uri (uri);
		bookmark = nemo_bookmark_new (location, NULL, NULL, NULL);

		if (!nemo_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
			nemo_bookmark_list_append (sidebar->bookmarks, bookmark);
		}

		g_object_unref (location);
		g_object_unref (bookmark);
		g_free (uri);
	}
}

static void
add_shortcut_cb (GtkAction           *item,
		 NemoPlacesSidebar *sidebar)
{
	add_bookmark (sidebar);
}

/* Rename the selected bookmark */
static void
rename_selected_bookmark (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	PlaceType type;
    GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    -1);

		if (type != PLACES_BOOKMARK) {
			return;
		}

		path = gtk_tree_model_get_path (model, &iter);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
		g_object_set (sidebar->editable_renderer, "editable", TRUE, NULL);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
						path, column, sidebar->editable_renderer, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
rename_shortcut_cb (GtkAction           *item,
		    NemoPlacesSidebar *sidebar)
{
	rename_selected_bookmark (sidebar);
}

/* Removes the selected bookmarks */
static void
remove_selected_bookmarks (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type;
	int index;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type != PLACES_BOOKMARK) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_INDEX, &index,
			    -1);

    if (index < sidebar->bookmark_breakpoint)
       decrement_bookmark_breakpoint (sidebar);

    nemo_bookmark_list_delete_item_at (sidebar->bookmarks, index);
}

static void
remove_shortcut_cb (GtkAction           *item,
		    NemoPlacesSidebar *sidebar)
{
	remove_selected_bookmarks (sidebar);
}

static void
mount_shortcut_cb (GtkAction           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GVolume *volume;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    -1);

	if (volume != NULL) {
		nemo_file_operations_mount_volume (NULL, volume);
		g_object_unref (volume);
	}
}

static void
unmount_done (gpointer data)
{
	NemoWindow *window;

	window = data;
	g_object_unref (window);
}

static void
show_unmount_progress_cb (GMountOperation *op,
                              const gchar *message,
                                    gint64 time_left,
                                    gint64 bytes_left,
                                  gpointer user_data)
{
    NemoApplication *app = NEMO_APPLICATION (g_application_get_default ());

    if (bytes_left == 0) {
        nemo_application_notify_unmount_done (app, message);
    } else {
        nemo_application_notify_unmount_show (app, message);
    }
}

static void
show_unmount_progress_aborted_cb (GMountOperation *op,
                                  gpointer user_data)
{
    NemoApplication *app = NEMO_APPLICATION (g_application_get_default ());
    nemo_application_notify_unmount_done (app, NULL);
}

static GMountOperation *
get_unmount_operation (NemoPlacesSidebar *sidebar)
{
    GMountOperation *mount_op;

    mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
    g_signal_connect (mount_op, "show-unmount-progress",
                      G_CALLBACK (show_unmount_progress_cb), sidebar);
    g_signal_connect (mount_op, "aborted",
                      G_CALLBACK (show_unmount_progress_aborted_cb), sidebar);

    return mount_op;
}

static void
do_unmount (GMount *mount,
	    NemoPlacesSidebar *sidebar)
{
    GMountOperation *mount_op;

	if (mount != NULL) {
        mount_op = get_unmount_operation (sidebar);
        nemo_file_operations_unmount_mount_full (NULL, mount, mount_op, FALSE, TRUE,
                                                 unmount_done,
                                                 g_object_ref (sidebar->window));
        g_object_unref (mount_op);
	}
}

static void
do_unmount_selection (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    -1);

	if (mount != NULL) {
		do_unmount (mount, sidebar);
		g_object_unref (mount);
	}
}

static void
unmount_shortcut_cb (GtkAction           *item,
		     NemoPlacesSidebar *sidebar)
{
	do_unmount_selection (sidebar);
}

static void
handle_mount_unmount_failure (const gchar *primary,
                              GError      *error)
{
    const gchar *message = NULL;

    if (error && error->code == G_IO_ERROR_FAILED_HANDLED) {
        return;
    }

    if (error) {
        message = error->message;
    }

    eel_show_error_dialog (primary,
                           message,
                           NULL);
}

static void
drive_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error)) {
        char *name, *primary;

        name = g_drive_get_name (G_DRIVE (source_object));
        primary = g_strdup_printf (_("Unable to eject %s"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
volume_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error)) {
        char *name, *primary;

        name = g_volume_get_name (G_VOLUME (source_object));
        primary = g_strdup_printf (_("Unable to eject %s"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
mount_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error)) {
        char *name, *primary;

        name = g_mount_get_name (G_MOUNT (source_object));
        primary = g_strdup_printf (_("Unable to eject %s"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
do_eject (GMount *mount,
	  GVolume *volume,
	  GDrive *drive,
	  NemoPlacesSidebar *sidebar)
{
    GMountOperation *mount_op = get_unmount_operation (sidebar);

	if (mount != NULL) {
		g_mount_eject_with_operation (mount, 0, mount_op, NULL, mount_eject_cb,
					      g_object_ref (sidebar->window));
	} else if (volume != NULL) {
		g_volume_eject_with_operation (volume, 0, mount_op, NULL, volume_eject_cb,
					      g_object_ref (sidebar->window));
	} else if (drive != NULL) {
		g_drive_eject_with_operation (drive, 0, mount_op, NULL, drive_eject_cb,
					      g_object_ref (sidebar->window));
	}
	g_object_unref (mount_op);
}

static void
eject_shortcut_cb (GtkAction           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	do_eject (mount, volume, drive, sidebar);

    g_clear_object (&mount);
    g_clear_object (&volume);
    g_clear_object (&drive);
}

static gboolean
eject_or_unmount_bookmark (NemoPlacesSidebar *sidebar,
			   GtkTreePath *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean can_unmount, can_eject;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;
	gboolean ret;

	model = gtk_tree_view_get_model (sidebar->tree_view);

	if (!path) {
		return FALSE;
	}
	if (!gtk_tree_model_get_iter (model, &iter, path)) {
		return FALSE;
	}

	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	ret = FALSE;

	check_unmount_and_eject (mount, volume, drive, &can_unmount, &can_eject);
	/* if we can eject, it has priority over unmount */
	if (can_eject) {
		do_eject (mount, volume, drive, sidebar);
		ret = TRUE;
	} else if (can_unmount) {
		do_unmount (mount, sidebar);
		ret = TRUE;
	}

    g_clear_object (&mount);
    g_clear_object (&volume);
    g_clear_object (&drive);

	return ret;
}

static gboolean
eject_or_unmount_selection (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	gboolean ret;

	if (!get_selected_iter (sidebar, &iter)) {
		return FALSE;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
	if (path == NULL) {
		return FALSE;
	}

	ret = eject_or_unmount_bookmark (sidebar, path);

	gtk_tree_path_free (path);

	return ret;
}

static void
drive_poll_for_media_cb (GObject *source_object,
			 GAsyncResult *res,
			 gpointer user_data)
{
	GError *error;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
        char *name, *primary;

        name = g_drive_get_name (G_DRIVE (source_object));
        primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
rescan_shortcut_cb (GtkAction           *item,
		    NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, NULL);
        g_object_unref (drive);
	}
}

static void
drive_start_cb (GObject      *source_object,
		GAsyncResult *res,
		gpointer      user_data)
{
	GError *error;

	error = NULL;

	if (!g_drive_start_finish (G_DRIVE (source_object), res, &error)) {
        char *name, *primary;

        name = g_drive_get_name (G_DRIVE (source_object));
        primary = g_strdup_printf (_("Unable to start %s"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
start_shortcut_cb (GtkAction           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		GMountOperation *mount_op;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));

		g_drive_start (drive, G_DRIVE_START_NONE, mount_op, NULL, drive_start_cb, NULL);

		g_object_unref (mount_op);
        g_object_unref (drive);
	}
}

static void
drive_stop_cb (GObject *source_object,
	       GAsyncResult *res,
	       gpointer user_data)
{
	NemoWindow *window;
	GError *error;

	window = user_data;
	g_object_unref (window);

	error = NULL;

    if (!g_drive_stop_finish (G_DRIVE (source_object), res, &error)) {
        char *name, *primary;

        name = g_drive_get_name (G_DRIVE (source_object));
        primary = g_strdup_printf (_("Unable to stop %s"), name);

        handle_mount_unmount_failure (primary, error);

        g_free (name);
        g_free (primary);
        g_clear_error (&error);
    }
}

static void
stop_shortcut_cb (GtkAction           *item,
		  NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter_for_store (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
        GMountOperation *mount_op = get_unmount_operation (sidebar);
		g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
			      g_object_ref (sidebar->window));
		g_object_unref (mount_op);
        g_object_unref (drive);
	}
}

static void
empty_trash_cb (GtkAction           *item,
		NemoPlacesSidebar *sidebar)
{
	nemo_file_operations_empty_trash (GTK_WIDGET (sidebar->window));
}

static gboolean
select_first_child_row_or_expand (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    GtkTreeIter child_iter;
	gboolean res;

	GtkTreePath *path = gtk_tree_model_get_path (model, iter);
	if (!path)
        return FALSE;
	gboolean expanded = gtk_tree_view_row_expanded(sidebar->tree_view, path);

	if(expanded) {
	  res = gtk_tree_model_iter_children(model, &child_iter, iter);
	  if(res) {
        *iter = child_iter;
        gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
	  }
	} else
	  gtk_tree_view_expand_row(sidebar->tree_view, path, FALSE);

	gtk_tree_path_free (path);
    return TRUE;
}

static gboolean
goto_parent_or_collapse (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    GtkTreeIter parent_iter;
	gboolean res=TRUE;

	GtkTreePath *path = gtk_tree_model_get_path (model, iter);
	if (!path)
        return FALSE;
	gboolean expanded = gtk_tree_view_row_expanded(sidebar->tree_view, path);

	if(expanded)
        gtk_tree_view_collapse_row(sidebar->tree_view, path);
	else {
	    res = gtk_tree_model_iter_parent(model, &parent_iter, iter);
		if(res) {
			*iter = parent_iter;
			gtk_tree_path_free (path);
			path = gtk_tree_model_get_path (model, iter);
		    gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
		}
	}

	gtk_tree_path_free (path);
    return res;
}


static gboolean
find_prev_or_next_row (NemoPlacesSidebar *sidebar,
		       GtkTreeIter *iter,
		       gboolean go_up)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
	gboolean res;
	GtkTreeIter copy;
	int place_type;

	copy = *iter;

	if (go_up) {
		res = gtk_tree_model_iter_previous (model, &copy);
	} else {
		res = gtk_tree_model_iter_next (model, &copy);
	}

	if (res) {
		gtk_tree_model_get (model, &copy,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    -1);
		if (place_type == PLACES_HEADING) {
			if (go_up) {
				res = gtk_tree_model_iter_previous (model, &copy);
			} else {
				res = gtk_tree_model_iter_next (model, &copy);
			}
		}
	}

	if (res)
      *iter = copy;
	return res;
}

static gboolean
goto_first_child_if_expanded (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    GtkTreePath *path;
    GtkTreeIter child_iter;

    // Get the tree path of the current node
    path = gtk_tree_model_get_path(model, iter);
    if (!path)
        return FALSE;

    // Check if this node is currently expanded in the tree view
    gboolean expanded = gtk_tree_view_row_expanded(sidebar->tree_view, path);

    if (!expanded) {
        gtk_tree_path_free(path);
        return FALSE;  // not expanded → stop here
    }

    // Try to get the first child (works only if the node is expanded)
    gboolean has_child = gtk_tree_model_iter_children(model, &child_iter, iter);

    gtk_tree_path_free(path);

    if (!has_child)
        return FALSE;

    // Replace the current iterator with the iterator of the first child
    *iter = child_iter;
    return TRUE;
}

static gboolean
goto_prev_row_or_last_child_if_expanded (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    GtkTreeIter prev_iter;
    GtkTreeIter current_iter;
    GtkTreeIter deepest_iter;

    // Try to move to the previous sibling row
    if (!gtk_tree_model_iter_previous(model, iter)) {
        return FALSE; // No previous sibling → nothing to do
    }
    prev_iter = *iter;

    // Get the GtkTreePath for the new current row (the previous one)
    GtkTreePath *path = gtk_tree_model_get_path(model, iter);
    if (!path)
        return FALSE;

    // Check if the previous row is expanded in the tree view
    gboolean expanded = gtk_tree_view_row_expanded(sidebar->tree_view, path);
    gtk_tree_path_free(path);

    if (!expanded) {
        // Not expanded → we stop here (iter already points to it)
        return TRUE;
    }

    // The previous row is expanded → go down to its last visible child (recursively)
    if (!gtk_tree_model_iter_children(model, &current_iter, &prev_iter)) {
        return TRUE; // Expanded but has no children
    }

    // Start with the first child
    deepest_iter = current_iter;

    // Iterate through all children to find the last one
    GtkTreeIter next_iter = current_iter;
    while (gtk_tree_model_iter_next(model, &next_iter)) {
        deepest_iter = next_iter;
    }

    // Now check if the deepest_iter has children and is expanded
    GtkTreePath *deepest_path = gtk_tree_model_get_path(model, &deepest_iter);
    if (deepest_path) {
        gboolean deepest_expanded = gtk_tree_view_row_expanded(sidebar->tree_view, deepest_path);
        gtk_tree_path_free(deepest_path);

        // If the deepest child is expanded, recursively find its last child
        while (deepest_expanded) {
            GtkTreeIter child_iter;
            if (!gtk_tree_model_iter_children(model, &child_iter, &deepest_iter)) {
                break; // No more children
            }

            // Find the last child of the current deepest_iter
            deepest_iter = child_iter;
            GtkTreeIter temp_iter = child_iter;
            while (gtk_tree_model_iter_next(model, &temp_iter)) {
                deepest_iter = temp_iter;
            }

            // Check if the new deepest_iter is expanded
            deepest_path = gtk_tree_model_get_path(model, &deepest_iter);
            if (deepest_path) {
                deepest_expanded = gtk_tree_view_row_expanded(sidebar->tree_view, deepest_path);
                gtk_tree_path_free(deepest_path);
            } else {
                deepest_expanded = FALSE;
            }
        }
    }

    // Replace current iter with the deepest child
    *iter = deepest_iter;
    return TRUE;
}


static gboolean
iter_equal(GtkTreeModel *model,
           GtkTreeIter *a,
           GtkTreeIter *b)
{
    GtkTreePath *path_a = gtk_tree_model_get_path(model, a);
    GtkTreePath *path_b = gtk_tree_model_get_path(model, b);

    gboolean equal = gtk_tree_path_compare(path_a, path_b) == 0;

    gtk_tree_path_free(path_a);
    gtk_tree_path_free(path_b);

    return equal;
}

static gboolean
find_parent_or_next_sibling_row(NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
    GtkTreeIter current_iter = *iter;

    while (TRUE) {
        GtkTreeIter parent_iter;
        if (!gtk_tree_model_iter_parent(model, &parent_iter, &current_iter)) {
            // no parent → no higher sibling found
			*iter = current_iter;
			if (gtk_tree_model_iter_next(model, &current_iter)) {
                    *iter = current_iter;
                    return TRUE;
			}
            return FALSE;
        }

        // Search for the next visible sibling beside the parent
        GtkTreeIter child_iter;
        if (!gtk_tree_model_iter_children(model, &child_iter, &parent_iter)) {
            current_iter = parent_iter;
            continue; // No children → one level up
        }

        do {
            if (iter_equal(model, &child_iter, &current_iter)) {
                // We are at the current iterator → check next child
                if (gtk_tree_model_iter_next(model, &child_iter)) {
                    *iter = child_iter;
                    return TRUE;
                } else {
                    break; // no mre siblings -> next higher level
                }
            }
        } while (gtk_tree_model_iter_next(model, &child_iter));

        // Next sibling not found on this level → one level higher
        current_iter = parent_iter;
    }
}


static gboolean
select_prev_or_next_node (NemoPlacesSidebar *sidebar,
		       GtkTreeIter *iter,
		       gboolean go_up)
{
	GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
	gboolean res;
	GtkTreeIter iterCopy;

	iterCopy = *iter;

	if (go_up) {
		res = goto_prev_row_or_last_child_if_expanded (sidebar, &iterCopy);
		if(!res) {
			// go to parent
			iterCopy = *iter;
            res = gtk_tree_model_iter_parent(model,  &iterCopy, iter);
		}
	} else {
		res = goto_first_child_if_expanded (sidebar, &iterCopy);
		if(!res) {
			// go to next child in same row
			iterCopy = *iter;
			res = gtk_tree_model_iter_next (model, &iterCopy);
			if(!res) {
				iterCopy = *iter;
				res = find_parent_or_next_sibling_row(sidebar, &iterCopy);
				if(!res)
					iterCopy = *iter;
			}
		}
	}
	if (res) {
          *iter = iterCopy;
          GtkTreePath *path = gtk_tree_model_get_path (model, &iterCopy);

          GtkTreeSelection *sel = gtk_tree_view_get_selection(sidebar->tree_view);
          gtk_tree_selection_unselect_all(sel);
          gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
          gtk_tree_path_free (path);
	}
	return res;

}


static gboolean
find_next_row (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	return find_prev_or_next_row (sidebar, iter, FALSE);
}

static void
properties_cb (GtkAction           *item,
	           NemoPlacesSidebar *sidebar)
{
	GList *list;
    if(sidebar->popup_file == NULL) return;
	list = g_list_prepend (NULL, nemo_file_ref (sidebar->popup_file));

	nemo_properties_window_present (list, GTK_WIDGET (sidebar->tree_view), NULL);

    nemo_file_list_free (list);
}

static gboolean
nemo_places_sidebar_focus (GtkWidget *widget,
			       GtkDirectionType direction)
{
	NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (widget);
	GtkTreePath *path;
	GtkTreeIter iter, child_iter;
	gboolean res;
    GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
	res = get_selected_iter (sidebar, &iter);
	if (!res) {
		gtk_tree_model_get_iter_first (model, &iter);
        gtk_tree_model_iter_children (model, &child_iter, &iter);
		res = find_next_row (sidebar, &child_iter);
		if (res) {
			path = gtk_tree_model_get_path (model, &iter);
			gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
	}

	return GTK_WIDGET_CLASS (nemo_places_sidebar_parent_class)->focus (widget, direction);
}

/* Handler for GtkWidget::key-press-event on the shortcuts list */
static gboolean
bookmarks_key_press_event_cb (GtkWidget             *widget,
			      GdkEventKey           *event,
			      NemoPlacesSidebar *sidebar)
{
  guint modifiers;
  GtkTreeIter selected_iter;

  switch (event->keyval) {
 	  case GDK_KEY_slash:
	  case GDK_KEY_KP_Divide:
	  case GDK_KEY_asciitilde:
          if (gtk_bindings_activate_event(G_OBJECT(sidebar->window), event))
              return GDK_EVENT_STOP;
          break;
	  default:
          break;
  }

  if (!get_selected_iter (sidebar, &selected_iter))
	  return FALSE;

  modifiers = gtk_accelerator_get_default_mod_mask ();

  switch(event->keyval) {
	  case GDK_KEY_Return:
	  case GDK_KEY_KP_Enter:
	  case GDK_KEY_ISO_Enter:
	  case GDK_KEY_space:
	      NemoWindowOpenFlags flags = 0;
          if ((event->state & modifiers) == GDK_SHIFT_MASK) {
              flags = NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
          } else if ((event->state & modifiers) == GDK_CONTROL_MASK) {
              flags = NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
          }
		  GtkTreeModel *model = gtk_tree_view_get_model (sidebar->tree_view);
          open_selected_bookmark (sidebar, model,
					  &selected_iter, flags);
          return TRUE;
	      break;
	  case GDK_KEY_Down:
          if ((event->state & modifiers) == GDK_MOD1_MASK) {
               return eject_or_unmount_selection (sidebar);
		  } else {
			  if ((event->state & modifiers) == 0) {
		         select_prev_or_next_node (sidebar, &selected_iter, FALSE);
			     return TRUE;
			  }
          }
		  break;
	  case GDK_KEY_Up:
          if ((event->state & modifiers) == 0) {
            select_prev_or_next_node (sidebar, &selected_iter, TRUE);
            return TRUE;
		  }
          break;
	  case GDK_KEY_F2:
          if ((event->state & modifiers) == 0) {
              rename_selected_bookmark (sidebar);
              return TRUE;
          }
          break;
	  case GDK_KEY_Left:
	      if ((event->state & modifiers) == 0) {
	        goto_parent_or_collapse(sidebar, &selected_iter);
            return TRUE;
		  }
          break;
	  case GDK_KEY_Right:
	      if ((event->state & modifiers) == 0) {
	        select_first_child_row_or_expand(sidebar, &selected_iter);
            return TRUE;
		  }
          break;
	  default:
	      return FALSE;
  }
  return FALSE;
}

static void
run_action_callback (GtkAction *action, gpointer user_data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);
    gchar *uri = NULL;
    GtkTreeIter iter;

    if (get_selected_iter_for_store (sidebar, &iter)) {
        gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
                    PLACES_SIDEBAR_COLUMN_URI, &uri,
                    -1);
    }

    if (!uri) {
        return;
    }

    NemoFile *file = nemo_file_get_by_uri (uri);
    NemoFile *parent = nemo_file_get_parent (file);
    GList *selection = g_list_prepend (NULL, file);

    nemo_action_activate (NEMO_ACTION (action), selection, parent, GTK_WINDOW (sidebar->window));

    nemo_file_list_free (selection);
    nemo_file_unref (parent);

    g_free (uri);
}

static gboolean
free_popup_file_in_idle_cb (gpointer user_data)
{
	NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

	if (sidebar->popup_file != NULL) {
		nemo_file_unref (sidebar->popup_file);
		sidebar->popup_file = NULL;
	}
	sidebar->popup_file_idle_handler = 0;
	return FALSE;
}

static void
popup_menu_deactivated (GtkMenuShell *menu_shell, gpointer user_data)
{
	NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

	/* The popup menu is deactivated. (I.E. hidden)
	   We want to free popup_file, but can't right away as it might immediately get
	   used if we're deactivation due to activating a menu item. So, we free it in
	   idle */

	if (sidebar->popup_file != NULL &&
	    sidebar->popup_file_idle_handler == 0) {
		sidebar->popup_file_idle_handler = g_idle_add (free_popup_file_in_idle_cb, sidebar);
	}
}

#if GTK_CHECK_VERSION (3, 24, 8)
static void
moved_to_rect_cb (GdkWindow          *window,
                  const GdkRectangle *flipped_rect,
                  const GdkRectangle *final_rect,
                  gboolean            flipped_x,
                  gboolean            flipped_y,
                  GtkMenu            *menu)
{
    g_signal_emit_by_name (menu,
                           "popped-up",
                           0,
                           flipped_rect,
                           final_rect,
                           flipped_x,
                           flipped_y);

    // Don't let the emission run in gtkmenu.c
    g_signal_stop_emission_by_name (window, "moved-to-rect");
}

static void
popup_menu_realized (GtkWidget    *menu,
                     gpointer      user_data)
{
    GdkWindow *toplevel;

    toplevel = gtk_widget_get_window (gtk_widget_get_toplevel (menu));

    g_signal_handlers_disconnect_by_func (toplevel, moved_to_rect_cb, menu);

    g_signal_connect (toplevel, "moved-to-rect", G_CALLBACK (moved_to_rect_cb),
                      menu);
}
#endif

static void
bookmarks_popup_menu (NemoPlacesSidebar *sidebar,
		      GdkEventButton        *event)
{
	char *uri = NULL;
	GtkTreeIter iter;

    update_menu_states (sidebar);
	if (get_selected_iter_for_store (sidebar, &iter)) {
		uri = get_fm_tree_model_uri (sidebar, sidebar->store, &iter);
		if(uri!=NULL) {
			NemoFile *file = nemo_file_get_by_uri (uri);
			sidebar->popup_file = nemo_file_ref (file);
			nemo_file_unref (file);
			g_free (uri);
		}
	}
    eel_pop_up_context_menu (GTK_MENU(sidebar->popup_menu),
                             (GdkEvent *) event,
                             GTK_WIDGET (sidebar));
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
			 NemoPlacesSidebar *sidebar)
{
	bookmarks_popup_menu (sidebar, NULL);
	return TRUE;
}

static void
reset_menu (NemoPlacesSidebar *sidebar)
{
    sidebar->actions_need_update = TRUE;
    rebuild_menu (sidebar);
}

static gboolean
actions_changed_idle_cb (gpointer user_data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    reset_menu (sidebar);

    sidebar->actions_changed_idle_id = 0;
    return G_SOURCE_REMOVE;
}

static void
actions_changed (gpointer user_data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    g_clear_handle_id (&sidebar->actions_changed_idle_id, g_source_remove);
    sidebar->actions_changed_idle_id = g_idle_add (actions_changed_idle_cb, sidebar);
}

static void
add_action_to_ui (NemoActionManager    *manager,
                  GtkAction            *action,
                  GtkUIManagerItemType  type,
                  const gchar          *path,
                  const gchar          *accelerator,
                  gpointer              user_data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    static const gchar *roots[] = {
        "/selection/PlacesSidebarActionsPlaceholder",
        NULL,
    };

    nemo_action_manager_add_action_ui (manager,
                                       sidebar->ui_manager,
                                       action,
                                       path,
                                       accelerator,
                                       sidebar->action_action_group,
                                       sidebar->action_action_group_merge_id,
                                       roots,
                                       type,
                                       G_CALLBACK (run_action_callback),
                                       sidebar);
}

static void
clear_ui (NemoPlacesSidebar *sidebar)
{

    nemo_ui_unmerge_ui (sidebar->ui_manager,
                        &sidebar->bookmark_action_group_merge_id,
                        &sidebar->bookmark_action_group);

    nemo_ui_unmerge_ui (sidebar->ui_manager,
                        &sidebar->action_action_group_merge_id,
                        &sidebar->action_action_group);

    nemo_ui_unmerge_ui (sidebar->ui_manager,
		                &sidebar->tv_action_group_merge_id,
		                &sidebar->tv_action_group);

}

static void
new_folder_done (GFile *new_folder,
		 gboolean success,
		 gpointer data)
{
	GList *list;

	/* show the properties window for the newly created
	 * folder so the user can change its name
	 */
	list = g_list_prepend (NULL, nemo_file_get (new_folder));

	nemo_properties_window_present (list, GTK_WIDGET (data), NULL);

        nemo_file_list_free (list);
}

static void
create_folder_cb (GtkAction *action,
			       NemoPlacesSidebar *sidebar)
{
	char *parent_uri;
	if(sidebar->popup_file == NULL) return;
	parent_uri = nemo_file_get_uri (sidebar->popup_file);
	nemo_file_operations_new_folder (GTK_WIDGET (sidebar->tree_view),
					     NULL,
					     parent_uri,
					     new_folder_done, sidebar->tree_view);

	g_free (parent_uri);
}

static const GtkActionEntry bookmark_action_entries[] = {
    { NEMO_ACTION_OPEN,                    "xsi-folder-open-symbolic", N_("_Open"),                NULL, NULL, G_CALLBACK (open_shortcut_cb)               },
    { NEMO_ACTION_OPEN_IN_NEW_TAB,         NULL,                   N_("Open in New _Tab"),     NULL, NULL, G_CALLBACK (open_shortcut_in_new_tab_cb)    },
    { NEMO_ACTION_OPEN_ALTERNATE,          NULL,                   N_("Open in New _Window"),  NULL, NULL, G_CALLBACK (open_shortcut_in_new_window_cb) },
    { NEMO_ACTION_ADD_BOOKMARK,            NULL,                   N_("_Add Bookmark"),        NULL, NULL, G_CALLBACK (add_shortcut_cb)                },
    { NEMO_ACTION_SIDEBAR_REMOVE,          "xsi-list-remove-symbolic", N_("Remove"),               NULL, NULL, G_CALLBACK (remove_shortcut_cb)             },
    { NEMO_ACTION_RENAME,                  NULL,                   N_("_Rename..."),           NULL, NULL, G_CALLBACK (rename_shortcut_cb)             },
    { NEMO_ACTION_MOUNT_VOLUME,            NULL,                   N_("_Mount"),               NULL, NULL, G_CALLBACK (mount_shortcut_cb)              },
    { NEMO_ACTION_UNMOUNT_VOLUME,          NULL,                   N_("_Unmount"),             NULL, NULL, G_CALLBACK (unmount_shortcut_cb)            },
    { NEMO_ACTION_EJECT_VOLUME,            NULL,                   N_("_Eject"),               NULL, NULL, G_CALLBACK (eject_shortcut_cb)              },
    { NEMO_ACTION_SIDEBAR_DETECT_MEDIA,    NULL,                   N_("_Detect Media"),        NULL, NULL, G_CALLBACK (rescan_shortcut_cb)             },
    { NEMO_ACTION_START_VOLUME,            NULL,                   N_("_Start"),               NULL, NULL, G_CALLBACK (start_shortcut_cb)              },
    { NEMO_ACTION_STOP_VOLUME,             NULL,                   N_("_Stop"),                NULL, NULL, G_CALLBACK (stop_shortcut_cb)               },
    { NEMO_ACTION_EMPTY_TRASH_CONDITIONAL, NULL,                   N_("_Empty _Trash"),        NULL, NULL, G_CALLBACK (empty_trash_cb)                 },
    { NEMO_ACTION_PROPERTIES,              NULL,                   N_("_Properties"),          NULL, NULL, G_CALLBACK (properties_cb)                  },
};

static const GtkActionEntry tree_sidebar_menu_entries[] = {
    { NEMO_ACTION_OPEN,                    "folder-open-symbolic",        N_("_Open"),                NULL, NULL, G_CALLBACK (open_shortcut_cb)               },
    { NEMO_ACTION_OPEN_IN_NEW_TAB,         NULL,                          N_("Open in New _Tab"),     NULL, NULL, G_CALLBACK (open_shortcut_in_new_tab_cb)    },
    { NEMO_ACTION_OPEN_ALTERNATE,          NULL,                          N_("Open in New _Window"),  NULL, NULL, G_CALLBACK (open_shortcut_in_new_window_cb) },
    { NEMO_ACTION_NEW_FOLDER,              NULL,                          N_("Create New _Folder"),   NULL, NULL, G_CALLBACK (create_folder_cb)      },
    { NEMO_ACTION_CUT,                     "edit-cut-symbolic",           N_("Cu_t"),                 NULL, NULL, G_CALLBACK (fm_tree_view_cut_cb)                },
    { NEMO_ACTION_COPY,                    "edit-copy-symbolic",          N_("_Copy"),                NULL, NULL, G_CALLBACK (fm_tree_view_copy_cb)               },
    { NEMO_ACTION_PASTE,                   "edit-paste-symbolic",         N_("_Paste Into Folder"),   NULL, NULL, G_CALLBACK (fm_tree_view_paste_cb)              },
    { NEMO_ACTION_PIN_FILE,                "xapp-pin-symbolic",           N_("P_in"),                 NULL, NULL, G_CALLBACK (fm_tree_view_pin_unpin_cb)          },
    { NEMO_ACTION_UNPIN_FILE,              "xapp-unpin-symbolic",         N_("Unp_in"),               NULL, NULL, G_CALLBACK (fm_tree_view_pin_unpin_cb)          },
    { NEMO_ACTION_TRASH,                   "user-trash-full-symbolic",    N_("Mo_ve to Trash"),       NULL, NULL, G_CALLBACK (fm_tree_view_trash_cb)              },
    { NEMO_ACTION_DELETE,                  "edit-delete-symbolic",        N_("_Delete"),              NULL, NULL, G_CALLBACK (fm_tree_view_delete_cb)             },
    { NEMO_ACTION_UNMOUNT_VOLUME,          NULL,                          N_("_Unmount"),             NULL, NULL, G_CALLBACK (unmount_shortcut_cb)            },
    { NEMO_ACTION_EJECT_VOLUME,            NULL,                          N_("_Eject"),               NULL, NULL, G_CALLBACK (eject_shortcut_cb)              },
    { NEMO_ACTION_PROPERTIES,             "document-properties-symbolic", N_("_Properties"),          NULL, NULL, G_CALLBACK (properties_cb)         },
};


static void
rebuild_menu (NemoPlacesSidebar *sidebar)
{
    if (!gtk_widget_get_realized (GTK_WIDGET (sidebar))) {
        return;
    }

    if (!sidebar->actions_need_update) {
        return;
    }

    clear_ui (sidebar);

    nemo_ui_prepare_merge_ui (sidebar->ui_manager,
                              "NemoPlacesSidebarBookmarkActions",
                              &sidebar->bookmark_action_group_merge_id,
                              &sidebar->bookmark_action_group);

    nemo_ui_prepare_merge_ui (sidebar->ui_manager,
                              "NemoPlacesSidebarActionActions",
                              &sidebar->action_action_group_merge_id,
                              &sidebar->action_action_group);

    nemo_ui_prepare_merge_ui (sidebar->ui_manager,
                              "NemoTreeviewSidebarFileActions",
                              &sidebar->tv_action_group_merge_id,
                              &sidebar->tv_action_group);

    sidebar->bookmark_action_group_merge_id =
            gtk_ui_manager_add_ui_from_resource (sidebar->ui_manager, "/org/nemo/nemo-places-sidebar-ui.xml", NULL);

    gtk_action_group_add_actions (sidebar->bookmark_action_group,
                                  bookmark_action_entries,
                                  G_N_ELEMENTS (bookmark_action_entries),
                                  sidebar);

    sidebar->tv_action_group_merge_id =
            gtk_ui_manager_add_ui_from_resource (sidebar->ui_manager, "/org/nemo/nemo-tree-sidebar-ui.xml", NULL);


    gtk_action_group_add_actions (sidebar->tv_action_group,
                                  tree_sidebar_menu_entries,
                                  G_N_ELEMENTS (tree_sidebar_menu_entries),
                                  sidebar);



    nemo_action_manager_iterate_actions (sidebar->action_manager,
                                         (NemoActionManagerIterFunc) add_action_to_ui,
                                         sidebar);

    if (sidebar->popup_menu == NULL) {
        GtkWidget *menu = gtk_ui_manager_get_widget (sidebar->ui_manager, "/selection");
        gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (sidebar->window)));
        sidebar->popup_menu = menu;

        g_signal_connect (sidebar->popup_menu, "deactivate",
                          G_CALLBACK (popup_menu_deactivated),
                          sidebar);

#if GTK_CHECK_VERSION (3, 24, 8)
        g_signal_connect (sidebar->popup_menu, "realize",
                          G_CALLBACK (popup_menu_realized),
                          sidebar);
        gtk_widget_realize (sidebar->popup_menu);
#endif

        gtk_widget_show (menu);
    }

    sidebar->actions_need_update = FALSE;
}

static gboolean
bookmarks_button_release_event_cb (GtkWidget *widget,
				   GdkEventButton *event,
				   NemoPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	gboolean res;

	path = NULL;

	if (event->type != GDK_BUTTON_RELEASE) {
		return TRUE;
	}

	if (clicked_eject_button (sidebar, &path)) {
		eject_or_unmount_bookmark (sidebar, path);
		gtk_tree_path_free (path);

		return FALSE;
	}

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);

	if (event->button == 1) {

		if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
			return FALSE;
		}

		res = gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
						     &path, NULL, NULL, NULL);

		if (!res) {
			return FALSE;
		}

		gtk_tree_model_get_iter (model, &iter, path);

		open_selected_bookmark (sidebar, model, &iter, 0);

		gtk_tree_path_free (path);
	}

	return FALSE;
}

static gboolean
bookmarks_button_press_event_cb (GtkWidget             *widget,
				 GdkEventButton        *event,
				 NemoPlacesSidebar *sidebar)

{
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	gboolean retval = FALSE;
	PlaceType row_type;

	if (event->type != GDK_BUTTON_PRESS) {
		/* ignore multiple clicks */
		return TRUE;
	}

	tree_view = GTK_TREE_VIEW (widget);
	model = gtk_tree_view_get_model (tree_view);
	gtk_tree_view_get_path_at_pos (tree_view, (int) event->x, (int) event->y,
				       &path, NULL, NULL, NULL);

	if (path == NULL || !gtk_tree_model_get_iter (model, &iter, path)) {
		return FALSE;
	}

	if (event->button == 3) {
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
				    -1);

		if (row_type != PLACES_HEADING) {
			bookmarks_popup_menu (sidebar, event);
		}
	} else if (event->button == 2) {
		NemoWindowOpenFlags flags = 0;

		if (g_settings_get_boolean (nemo_preferences,
					    NEMO_PREFERENCES_ALWAYS_USE_BROWSER)) {
			flags = (event->state & GDK_CONTROL_MASK) ?
				NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW :
				NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
		} else {
			flags = NEMO_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
		}

		open_selected_bookmark (sidebar, model, &iter, flags);
		retval = TRUE;
	}

	gtk_tree_path_free (path);

	return retval;
}

typedef struct {
    NemoPlacesSidebar *sidebar;
    GtkTreePath *hovered_path;
} ClearHoverData;

static gboolean
clear_eject_hover (GtkTreeModel *model,
                    GtkTreePath  *path,
                    GtkTreeIter  *iter,
                    gpointer      data)
{
    ClearHoverData *hdata = data;
    gint size;
    gboolean can_eject = FALSE;
	GtkTreeIter fm_iter;
	get_fm_tree_model_iter (model, iter, &fm_iter);
    gtk_tree_model_get (hdata->sidebar->store, &fm_iter,
                        PLACES_SIDEBAR_COLUMN_EJECT, &can_eject,
                        -1);

    if (can_eject && hdata->hovered_path != NULL && gtk_tree_path_compare (path, hdata->hovered_path) == 0) {
        size = EJECT_ICON_SIZE_HOVERED;

    } else {
        size = EJECT_ICON_SIZE_NOT_HOVERED;
    }
	if(hdata->sidebar->use_file_treeview) {
		fm_tree_model_set (hdata->sidebar->store, &fm_iter,
		                    PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE, size,
		                    -1);
	} else {
		gtk_tree_store_set (hdata->sidebar->store, &fm_iter,
		                    PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE, size,
		                    -1);
	}

    return FALSE;
}

static gboolean
motion_notify_cb (GtkWidget         *widget,
                  GdkEventMotion    *event,
                  NemoPlacesSidebar *sidebar)
{
    GtkTreeModel *model;
    GtkTreePath *path = NULL;
    GtkTreePath *store_path = NULL;
    gboolean editing;

    if (event->type != GDK_MOTION_NOTIFY) {
        return TRUE;
    }

    g_object_get (sidebar->editable_renderer, "editing", &editing, NULL);
    if (editing) {
        return GDK_EVENT_PROPAGATE;
    }

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (sidebar->tree_view));

    if (over_eject_button (sidebar, event->x, event->y, &path)) {
		if(sidebar->use_file_treeview) {
			store_path = path;
		} else {
        		store_path = gtk_tree_model_filter_convert_path_to_child_path (GTK_TREE_MODEL_FILTER (model), path);
			gtk_tree_path_free (path);
		}
    }

    ClearHoverData data = { sidebar, store_path };
    gtk_tree_model_foreach (GTK_TREE_MODEL (sidebar->store), (GtkTreeModelForeachFunc) clear_eject_hover, &data);

    if (store_path != NULL) {
        gtk_tree_path_free (store_path);
    }



    return FALSE;
}

static gboolean
leave_notify_cb (GtkWidget         *widget,
                 GdkEventCrossing  *event,
                 NemoPlacesSidebar *sidebar)
{
    gboolean editing;

    if (event->type != GDK_LEAVE_NOTIFY) {
        return TRUE;
    }

    g_object_get (sidebar->editable_renderer, "editing", &editing, NULL);
    if (editing) {
        return GDK_EVENT_PROPAGATE;
    }

    ClearHoverData data = { sidebar, NULL };
    gtk_tree_model_foreach (GTK_TREE_MODEL (sidebar->store), (GtkTreeModelForeachFunc) clear_eject_hover, &data);

    return FALSE;
}


static gboolean
query_tooltip_callback (GtkWidget *widget,
                        gint x,
                        gint y,
                        gboolean kb_mode,
                        GtkTooltip *tooltip,
                        gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreePath *path = NULL;
    GtkTreeModel *model;

    if (gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget), &x, &y,
                                           kb_mode,
                                           &model, &path, &iter)) {
        gboolean can_eject;
        gint icon_size, type;

        gtk_tree_model_get (model,
                            &iter,
                            PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                            PLACES_SIDEBAR_COLUMN_EJECT, &can_eject,
                            // HACK: If we store a bool 'hovered' we still need to have an
                            // icon size column or else make a cell_data_func to render the icon
                            // manually. This is simpler.
                            PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE, &icon_size,
                            -1);

        if (type == PLACES_HEADING) {
            gtk_tree_path_free (path);
            return FALSE;
        }

        g_autofree gchar *tooltip_markup = NULL;

        if (can_eject && icon_size == EJECT_ICON_SIZE_HOVERED) {
            g_autoptr(GMount) mount = NULL;
            g_autoptr(GDrive) drive = NULL;
            g_autoptr(GVolume) volume = NULL;

            gtk_tree_model_get (model,
                                &iter,
                                PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
                                PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
                                PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
                                -1);
            if (mount != NULL) {
                tooltip_markup = g_strdup (_("Unmount"));
            }
            else
            if (drive != NULL) {
                tooltip_markup = g_strdup (_("Eject"));
            }
            else
            if (volume != NULL) {
                tooltip_markup = g_strdup (_("Stop"));
            }
        } else {
            if (type == PLACES_BUILT_IN) {
                return FALSE;
            }
            gtk_tree_model_get (model,
                                &iter,
                                PLACES_SIDEBAR_COLUMN_TOOLTIP, &tooltip_markup,
                                -1);
        }

        gtk_tooltip_set_markup (tooltip, tooltip_markup);
        gtk_tree_view_set_tooltip_cell (GTK_TREE_VIEW (widget), tooltip, path, NULL, NULL);

        gtk_tree_path_free (path);
        return TRUE;
    }

    return FALSE;
}


static void
update_expanded_state (GtkTreeView *tree_view,
                       GtkTreeIter *iter,
                       GtkTreePath *path,
                           gpointer user_data,
                           gboolean expanded)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);

    if (sidebar->updating_sidebar)
        return;

    SectionType type;
    PlaceType   placetype;
    GtkTreeIter heading_iter;
    GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &heading_iter, path);
    gtk_tree_model_get (model, &heading_iter,
                    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &type,
                    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &placetype,
                    -1);
    if(placetype == PLACES_HEADING) {
        switch (type) {
            case SECTION_COMPUTER:
                sidebar->my_computer_expanded = expanded;
                g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_MY_COMPUTER_EXPANDED, expanded);
                break;
            case SECTION_BOOKMARKS:
                sidebar->bookmarks_expanded = expanded;
                g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_BOOKMARKS_EXPANDED, expanded);
                break;
            case SECTION_DEVICES:
                sidebar->devices_expanded = expanded;
                g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_DEVICES_EXPANDED, expanded);
                break;
            case SECTION_NETWORK:
                sidebar->network_expanded = expanded;
                g_settings_set_boolean (nemo_window_state, NEMO_WINDOW_STATE_NETWORK_EXPANDED, expanded);
                break;
            default:
                return;
        }
    }
}

static void
row_collapsed_cb (GtkTreeView *tree_view,
                              GtkTreeIter *iter,
                              GtkTreePath *path,
                              gpointer user_data)
{
    update_expanded_state (tree_view,
                           iter,
                           path,
                           user_data,
                           FALSE);
}


static void
row_expanded_cb (GtkTreeView *tree_view,
                              GtkTreeIter *iter,
                              GtkTreePath *path,
                              gpointer user_data)
{
    update_expanded_state (tree_view,
                           iter,
                           path,
                           user_data,
                           TRUE);
    GtkTreeSelection *sel = gtk_tree_view_get_selection(tree_view);
    gtk_tree_selection_unselect_all(sel);
    gtk_tree_selection_select_path(sel, path);
}

static void
row_activated_cb (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           user_data)
{
    GtkTreeIter iter;
    SectionType section_type;
    PlaceType place_type;

    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (user_data);
    GtkTreeModel *model = (GtkTreeModel *) (sidebar->store);
    gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	if(sidebar->use_file_treeview) {
	  if(!iter_is_valid(FM_TREE_MODEL(model), &iter))return;
	}
    gtk_tree_model_get (model, &iter,
                    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
                    -1);
    if (place_type == PLACES_HEADING) {
        if (section_type == SECTION_COMPUTER) {
            sidebar->my_computer_expanded = !sidebar->my_computer_expanded;
        } else if (section_type == SECTION_BOOKMARKS) {
            sidebar->bookmarks_expanded = !sidebar->bookmarks_expanded;
        } else if (section_type == SECTION_DEVICES) {
            sidebar->devices_expanded = !sidebar->devices_expanded;
        } else if (section_type == SECTION_NETWORK) {
            sidebar->network_expanded = !sidebar->network_expanded;
        }
        restore_expand_state (sidebar);
    }
}

static void
bookmarks_edited (GtkCellRenderer       *cell,
		  gchar                 *path_string,
		  gchar                 *new_text,
		  NemoPlacesSidebar *sidebar)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	NemoBookmark *bookmark;
	int index;

	g_object_set (cell, "editable", FALSE, NULL);

	path = gtk_tree_path_new_from_string (path_string);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store), &iter, path);
	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
		            PLACES_SIDEBAR_COLUMN_INDEX, &index,
		            -1);
	gtk_tree_path_free (path);
	bookmark = nemo_bookmark_list_item_at (sidebar->bookmarks, index);

	if (bookmark != NULL) {
		nemo_bookmark_set_custom_name (bookmark, new_text);
	}
}

static void
bookmarks_editing_canceled (GtkCellRenderer       *cell,
			    NemoPlacesSidebar *sidebar)
{
	g_object_set (cell, "editable", FALSE, NULL);
}

static void
trash_state_changed_cb (NemoTrashMonitor *trash_monitor,
			gboolean             state,
			gpointer             data)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (data);

	/* The trash icon changed, update the sidebar */
	update_places (sidebar);

	reset_menu (sidebar);
}

static void
favorites_changed_cb (gpointer data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (data);

    update_places (sidebar);
}

static gboolean
tree_selection_func (GtkTreeSelection *selection,
		     GtkTreeModel *model,
		     GtkTreePath *path,
		     gboolean path_currently_selected,
		     gpointer user_data)
{
	GtkTreeIter iter;
	PlaceType row_type;

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
			    -1);

	if (row_type == PLACES_HEADING) {
		return FALSE;
	}

	return TRUE;
}

static void
icon_cell_renderer_func (GtkTreeViewColumn *column,
			 GtkCellRenderer *cell,
			 GtkTreeModel *model,
			 GtkTreeIter *iter,
			 gpointer user_data)
{
	gboolean expanded;
	GIcon *gicon = NULL;
    g_object_get (cell,
              "is-expanded", &expanded,
              NULL);

	gtk_tree_model_get (model, iter,
                        expanded ? PLACES_SIDEBAR_OPEN_ICON_COLUMN : PLACES_SIDEBAR_COLUMN_GICON,
                        &gicon,
                        -1);

	if (gicon) {
		g_object_set(cell, "visible", TRUE, "gicon", gicon, NULL);
		g_object_unref(gicon);
	} else {
		g_object_set(cell, "visible", FALSE, NULL);
	}
}



static void
set_treeview_style(GtkTreeView *treeview, gboolean focused)
{
    GtkCssProvider *provider = gtk_css_provider_new();

    if (focused) {
        gtk_css_provider_load_from_data(provider,
            "treeview.view:selected { "
            "   background-color: @theme_selected_bg_color; "
            "   color: @theme_selected_fg_color; "
            "}", -1, NULL);
    } else {
        gtk_css_provider_load_from_data(provider,
            "treeview.view:selected { "
            "   background-color: #d0d0d0; "
            "   color: @theme_fg_color; "
            "}", -1, NULL);
    }

    GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(treeview));
    gtk_style_context_add_provider(context,
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

static gboolean
on_tree_focus_in(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    set_treeview_style(GTK_TREE_VIEW(widget), TRUE);
    return FALSE; // Keep default behavior
}

static gboolean
on_tree_focus_out(GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    set_treeview_style(GTK_TREE_VIEW(widget), FALSE);
    return FALSE; // Keep default behavior
}


static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
    GtkTreePath *path = gtk_tree_model_get_path(model, iter);
    int depth = gtk_tree_path_get_depth(path); /* Root = 1 */
    gtk_tree_path_free(path);

    g_object_set(cell,
                 "width", 3 + (depth - 1) * 0,  /* optional: leicht anpassen */
                 "visible", TRUE,
                 NULL);
}


static void
text_cell_renderer_func(GtkTreeViewColumn *column,
                        GtkCellRenderer   *renderer,
                        GtkTreeModel      *model,
                        GtkTreeIter       *iter,
                        gpointer           user_data)
{
    gchar *name_text = NULL;
    gchar *heading_text = NULL;
    gint row_type = 0;

    gtk_tree_model_get(model, iter,
                       PLACES_SIDEBAR_COLUMN_ROW_TYPE, &row_type,
                       PLACES_SIDEBAR_COLUMN_NAME, &name_text,
                       -1);

    if (row_type == PLACES_HEADING) {
        /* For Headings use HEADING_TEXT (bold) */
        g_free(name_text);

        gtk_tree_model_get(model, iter,
                           PLACES_SIDEBAR_COLUMN_HEADING_TEXT, &heading_text,
                           -1);

        g_object_set(renderer,
                     "weight", PANGO_WEIGHT_BOLD,
                     "weight-set", TRUE,
                     NULL);

        g_object_set(renderer, "text", heading_text ? heading_text : "", NULL);
        g_free(heading_text);
    } else {
        const gchar *display = "";
        if (name_text != NULL && *name_text != '\0')
            display = name_text;

        g_object_set(renderer,
                     "weight-set", FALSE,
                     NULL);

        g_object_set(renderer, "text", display, NULL);

        g_free(name_text);
    }
}

enum
{
  PROP_DISK_FULL_PERCENTAGE = 1,
  PROP_SHOW_DISK_FULL_PERCENTAGE = 2,
};

static void
nemo_places_tree_sidebar_renderer_init(NemoPlacesSidebar *sidebar, GtkTreeView *tree_view)
{
    GtkCellRenderer *cell;
    GtkTreeViewColumn *primary_column;

    /* === primary row === */
    primary_column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_expand(primary_column, TRUE);

    /* --- padding Cell renderer --- */
    cell = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(primary_column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func(primary_column, cell, padding_cell_renderer_func, sidebar, NULL);
    sidebar->padding_cell_renderer = cell;

    /* --- icon cell renderer --- */
    cell = gtk_cell_renderer_pixbuf_new();
    g_object_set(cell,
                 "xalign", 0.0,
                 "follow-state", TRUE,
                 NULL);
    gtk_tree_view_column_pack_start(primary_column, cell, FALSE);
    gtk_tree_view_column_set_cell_data_func(primary_column, cell, icon_cell_renderer_func, sidebar, NULL);
    sidebar->icon_cell_renderer = cell;

    /* --- text renderer --- */
	cell = sidebar->editable_renderer = nemo_cell_renderer_disk_new();
	NEMO_CELL_RENDERER_DISK(cell)->direction = gtk_widget_get_direction(GTK_WIDGET(tree_view));
	gtk_tree_view_column_pack_start(primary_column, cell, TRUE);
	g_object_set(cell, "editable", FALSE, NULL);

	/* remove 'text' here — we set text exclusively in text_cell_renderer_func */
	gtk_tree_view_column_set_attributes(primary_column, cell,
		                                "editable-set", PLACES_SIDEBAR_COLUMN_BOOKMARK,
		                                "disk-full-percent", PLACES_SIDEBAR_COLUMN_DF_PERCENT,
		                                "show-disk-full-percent", PLACES_SIDEBAR_COLUMN_SHOW_DF,
		                                NULL);

	/* set cell data func, which considers TREE_NAME and NAME  */
	gtk_tree_view_column_set_cell_data_func(primary_column, cell, text_cell_renderer_func, sidebar, NULL);

	g_signal_connect(cell, "edited", G_CALLBACK(bookmarks_edited), sidebar);
	g_signal_connect(cell, "editing-canceled", G_CALLBACK(bookmarks_editing_canceled), sidebar);

	gtk_tree_view_append_column(tree_view, primary_column);
    /* === Eject Column === */
    sidebar->eject_column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_sizing(sidebar->eject_column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_min_width(sidebar->eject_column, EJECT_COLUMN_MIN_WIDTH);
    gtk_tree_view_column_set_max_width(sidebar->eject_column, EJECT_COLUMN_MAX_WIDTH);

    /* Eject Icon Renderer */
    cell = gtk_cell_renderer_pixbuf_new();
    sidebar->eject_icon_cell_renderer = cell;
    g_object_set(cell,
                 "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
                 "yalign", 0.8,
                 "width", menu_icon_pixels,
                 NULL);
    gtk_tree_view_column_pack_start(sidebar->eject_column, cell, FALSE);
    gtk_tree_view_column_set_attributes(sidebar->eject_column, cell,
                                        "visible", PLACES_SIDEBAR_COLUMN_EJECT,
                                        "icon-name", PLACES_SIDEBAR_COLUMN_EJECT_ICON,
                                        "stock-size", PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE,
                                        NULL);

    gtk_tree_view_append_column(tree_view, sidebar->eject_column);

}

static void
hidden_files_mode_changed_callback (NemoWindow *window,
                    NemoPlacesSidebar *sidebar)
{
   update_filtering_from_preferences (sidebar);
}

static void
notify_clipboard_info (NemoClipboardMonitor *monitor,
                       NemoClipboardInfo *info,
                       NemoPlacesSidebar *sidebar)
{
	if (info != NULL && info->cut) {
		fm_tree_model_set_highlight_for_files ( FM_TREE_MODEL (sidebar->store), info->files);
	} else {
		fm_tree_model_set_highlight_for_files ( FM_TREE_MODEL (sidebar->store), NULL);
	}
}

static void
refresh_highlight (NemoPlacesSidebar *sidebar)
{
	NemoClipboardMonitor *monitor;
	NemoClipboardInfo *info;

	monitor = nemo_clipboard_monitor_get ();
	info = nemo_clipboard_monitor_get_clipboard_info (monitor);

	notify_clipboard_info (monitor, info, sidebar);
}






static int
compare_rows (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer callback_data)
{
	NemoFile *file_a, *file_b;
	int result;

	/* Dummy rows are always first */
	if (a->user_data == NULL) {
		return -1;
	}
	else if (b->user_data == NULL) {
		return 1;
	}

	/* don't sort root nodes */
	if (fm_tree_model_iter_is_root (FM_TREE_MODEL (model), a) &&
	    fm_tree_model_iter_is_root (FM_TREE_MODEL (model), b)) {
		return fm_tree_model_iter_compare_roots (FM_TREE_MODEL (model), a, b);
	}

	file_a = fm_tree_model_iter_get_file (FM_TREE_MODEL (model), a);
	file_b = fm_tree_model_iter_get_file (FM_TREE_MODEL (model), b);

	if (file_a == file_b) {
		result = 0;
	} else if (file_a == NULL) {
		result = -1;
	} else if (file_b == NULL) {
		result = +1;
	} else {
        result = nemo_file_compare_for_sort (file_a, file_b,
                                             NEMO_FILE_SORT_BY_DISPLAY_NAME,
                                             FALSE, //sidebar->sort_directories_first,
                                             FALSE, //sidebar->sort_favorites_first,
                                             FALSE,
                                             NULL);
	}

	nemo_file_unref (file_a);
	nemo_file_unref (file_b);

	return result;
}



static void
row_loaded_callback (GtkTreeModel     *tree_model,
		     GtkTreeIter      *iter,
		     NemoPlacesSidebar *sidebar)
{
	NemoFile *file, *tmp_file, *selection_file;

	if (sidebar->selection_location == NULL
	    || !sidebar->selecting
	    || iter->user_data == NULL || iter->stamp == 0) {
		return;
	}

	file = fm_tree_model_iter_get_file ( FM_TREE_MODEL (sidebar->store), iter);
	if (file == NULL) {
		return;
	}
	if (!nemo_file_is_directory (file)) {
		nemo_file_unref(file);
		return;
	}

	/* if iter is ancestor of wanted selection_location then update selection */
	selection_file = nemo_file_get_by_uri (sidebar->selection_location);
	while (selection_file != NULL) {
		if (file == selection_file) {
			nemo_file_unref (file);
			nemo_file_unref (selection_file);

			schedule_show_selection (sidebar);
			return;
		}
		tmp_file = nemo_file_get_parent (selection_file);
		nemo_file_unref (selection_file);
		selection_file = tmp_file;
	}
	nemo_file_unref (file);
}


static gboolean
row_visibility_function (GtkTreeModel *model,
                         GtkTreeIter  *iter,
                         gpointer      data)
{
    NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (data);

    SectionType section_type;
    PlaceType type;

    gtk_tree_model_get (model, iter,
                        PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
                        PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
                        -1);

    if (type != PLACES_HEADING || section_type != SECTION_BOOKMARKS)
        return TRUE;

    if (sidebar->in_drag)
        return TRUE;

    if ((int)nemo_bookmark_list_length (sidebar->bookmarks) > sidebar->bookmark_breakpoint)
        return TRUE;

    return FALSE;
}

static gint
get_icon_scale_callback (FMTreeModel *model,
                         NemoPlacesSidebar  *sidebar)
{
   return gtk_widget_get_scale_factor (GTK_WIDGET (sidebar->tree_view));
}

//static GdkAtom copied_files_atom;

static void
nemo_places_sidebar_init (NemoPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeSelection  *selection;
	GtkStyleContext   *style_context;

    sidebar->action_manager = nemo_action_manager_new ();
    sidebar->actions_changed_id = g_signal_connect_swapped (sidebar->action_manager,
                                                            "changed",
                                                            G_CALLBACK (actions_changed),
                                                            sidebar);


    sidebar->ui_manager = gtk_ui_manager_new ();

    sidebar->in_drag = FALSE;

    /* enables TreeView for files */
	sidebar->use_file_treeview = TRUE;

    sidebar->desktop_dnd_source_fs = NULL;
    sidebar->desktop_dnd_can_delete_source = TRUE;

    sidebar->update_places_on_idle_id = 0;

    sidebar->bookmarks_changed_id = 0;

    sidebar-> hidden_files_changed_id = 0;

	sidebar->volume_monitor = NULL;


    sidebar->my_computer_expanded = g_settings_get_boolean (nemo_window_state,
                                                            NEMO_WINDOW_STATE_MY_COMPUTER_EXPANDED);
    sidebar->bookmarks_expanded = g_settings_get_boolean (nemo_window_state,
                                                             NEMO_WINDOW_STATE_BOOKMARKS_EXPANDED);
    sidebar->devices_expanded = g_settings_get_boolean (nemo_window_state,
                                                        NEMO_WINDOW_STATE_DEVICES_EXPANDED);
    sidebar->network_expanded = g_settings_get_boolean (nemo_window_state,
                                                        NEMO_WINDOW_STATE_NETWORK_EXPANDED);

    gtk_widget_set_size_request (GTK_WIDGET (sidebar), 140, -1);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

	style_context = gtk_widget_get_style_context (GTK_WIDGET (sidebar));
	gtk_style_context_set_junction_sides (style_context, GTK_JUNCTION_RIGHT | GTK_JUNCTION_LEFT);

	/* Make it easier for theme authors to style the sidebar */
	gtk_style_context_add_class (style_context, "nemo-places-sidebar");


	if(sidebar->use_file_treeview)
	{
		sidebar->store = fm_tree_model_new();
		GType model_types[PLACES_SIDEBAR_COLUMN_COUNT] = {
			G_TYPE_STRING,      		// FM_TREE_MODEL_DISPLAY_NAME_COLUMN
			g_icon_get_type(),		// FM_TREE_MODEL_CLOSED_ICON_COLUMN
			g_icon_get_type(),		// FM_TREE_MODEL_OPEN_ICON_COLUMN
			PANGO_TYPE_STYLE,		// FM_TREE_MODEL_FONT_STYLE_COLUMN
			G_TYPE_INT,				// FM_TREE_MODEL_TEXT_WEIGHT_COLUMN
		    G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_ROW_TYPE
			G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_URI
			G_TYPE_DRIVE,			// PLACES_SIDEBAR_COLUMN_DRIVE
			G_TYPE_VOLUME,			// PLACES_SIDEBAR_COLUMN_VOLUME
			G_TYPE_MOUNT,			// PLACES_SIDEBAR_COLUMN_MOUNT
			G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_INDEX
			G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_EJECT
			G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_NO_EJECT
			G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_BOOKMARK
			G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_TOOLTIP
			G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_EJECT_ICON
		    G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE
			G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_SECTION_TYPE
			G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_HEADING_TEXT
		    G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_DF_PERCENT
			G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_SHOW_DF
			G_TYPE_STRING			// PLACES_SIDEBAR_COLUMN_TREE_NAME
		};
		fm_tree_model_set_column_types((FMTreeModel*)(sidebar->store), PLACES_SIDEBAR_COLUMN_COUNT, model_types);

		tree_view = GTK_TREE_VIEW (nemo_places_tree_view_new ());

		gtk_tree_view_set_headers_visible (tree_view, FALSE);

		sidebar->sort_model = (GtkTreeModel*)GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (sidebar->store)));

		gtk_tree_view_set_search_column (GTK_TREE_VIEW (tree_view), PLACES_SIDEBAR_COLUMN_NAME);

		gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sidebar->sort_model),
						 compare_rows, sidebar, NULL);

		gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->sort_model));

	} else {
		  	/* tree view */
		tree_view = GTK_TREE_VIEW (nemo_places_tree_view_new ());

		gtk_tree_view_set_headers_visible (tree_view, FALSE);

		sidebar->store = nemo_shortcuts_model_new (sidebar);

		sidebar->sort_model = (GtkTreeModel*)GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (sidebar->store)));

		gtk_tree_view_set_search_column (GTK_TREE_VIEW (tree_view), PLACES_SIDEBAR_COLUMN_NAME);

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sidebar->store),
		                                      PLACES_SIDEBAR_COLUMN_TREE_NAME,
		                                      GTK_SORT_ASCENDING);

		sidebar->store_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(sidebar->sort_model), NULL);

		gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(sidebar->store_filter),
				                               (GtkTreeModelFilterVisibleFunc) row_visibility_function,
				                               sidebar, NULL);

		gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store_filter));
	}

	sidebar->tree_view = tree_view;
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));

	gtk_widget_show (GTK_WIDGET (sidebar));

	nemo_places_tree_sidebar_renderer_init (sidebar, tree_view);

	gtk_tree_view_set_search_column (tree_view, PLACES_SIDEBAR_COLUMN_NAME);
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);


	gtk_tree_selection_set_select_function (selection,
						tree_selection_func,
						sidebar,
						NULL);

	gtk_tree_view_enable_model_drag_source (GTK_TREE_VIEW (tree_view),
						GDK_BUTTON1_MASK,
						nemo_shortcuts_source_targets,
						G_N_ELEMENTS (nemo_shortcuts_source_targets),
						GDK_ACTION_MOVE);

	gtk_tree_view_enable_model_drag_dest(
		GTK_TREE_VIEW(tree_view),
		nemo_shortcuts_drop_targets,
		G_N_ELEMENTS(nemo_shortcuts_drop_targets),
		GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK
	);

	g_signal_connect (tree_view, "key-press-event",
			  G_CALLBACK (bookmarks_key_press_event_cb), sidebar);

	g_signal_connect (tree_view, "drag-motion",
			  G_CALLBACK (drag_motion_callback), sidebar);
	g_signal_connect (tree_view, "drag-leave",
			  G_CALLBACK (drag_leave_callback), sidebar);
	g_signal_connect (tree_view, "drag-data-received",
			  G_CALLBACK (drag_data_received_callback), sidebar);
	g_signal_connect (tree_view, "drag-drop",
			  G_CALLBACK (drag_drop_callback), sidebar);

	g_signal_connect (selection, "changed",
			  G_CALLBACK (bookmarks_selection_changed_cb), sidebar);
	g_signal_connect (tree_view, "popup-menu",
			  G_CALLBACK (bookmarks_popup_menu_cb), sidebar);
	g_signal_connect (tree_view, "button-press-event",
			  G_CALLBACK (bookmarks_button_press_event_cb), sidebar);
	g_signal_connect (tree_view, "button-release-event",
			  G_CALLBACK (bookmarks_button_release_event_cb), sidebar);
    g_signal_connect (tree_view, "row-expanded",
              G_CALLBACK (row_expanded_cb), sidebar);
    g_signal_connect (tree_view, "row-collapsed",
              G_CALLBACK (row_collapsed_cb), sidebar);
    g_signal_connect (tree_view, "row-activated",
              G_CALLBACK (row_activated_cb), sidebar);
    g_signal_connect (tree_view, "motion-notify-event",
              G_CALLBACK (motion_notify_cb), sidebar);
    g_signal_connect (tree_view, "leave-notify-event",
              G_CALLBACK (leave_notify_cb), sidebar);

    g_signal_connect_object (GTK_WIDGET (tree_view), "query-tooltip",
                             G_CALLBACK (query_tooltip_callback), sidebar, 0);
    gtk_widget_set_has_tooltip (GTK_WIDGET (tree_view), TRUE);

	g_signal_connect(sidebar->tree_view, "focus-in-event",
		             G_CALLBACK(on_tree_focus_in), NULL);

	g_signal_connect(sidebar->tree_view, "focus-out-event",
		             G_CALLBACK(on_tree_focus_out), NULL);

	g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
				  G_CALLBACK(desktop_setting_changed_callback),
				  sidebar);

	g_signal_connect_swapped (nemo_desktop_preferences, "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
				  G_CALLBACK(desktop_setting_changed_callback),
				  sidebar);

    g_signal_connect_swapped (cinnamon_privacy_preferences, "changed::" NEMO_PREFERENCES_RECENT_ENABLED,
                  G_CALLBACK(desktop_setting_changed_callback),
                  sidebar);

	g_signal_connect_object (nemo_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (trash_state_changed_cb),
				 sidebar, 0);

    g_signal_connect_swapped (xapp_favorites_get_default (),
                              "changed",
                              G_CALLBACK (favorites_changed_cb),
                              sidebar);

	if(sidebar->use_file_treeview) {
		fm_tree_model_set_custom_row_draggable_func (sidebar->store, nemo_places_sidebar_row_draggable, sidebar);

		g_signal_connect_object (sidebar->store, "row_loaded",
		                     G_CALLBACK (row_loaded_callback),
                             sidebar, G_CONNECT_AFTER);

    		g_signal_connect_object (sidebar->store, "get-icon-scale",
                             G_CALLBACK (get_icon_scale_callback), sidebar, 0);
	}

}

static void
nemo_places_sidebar_dispose (GObject *object)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (object);
	tree_model_remove_all_nodes(sidebar);

	g_clear_object (&sidebar->desktop_dnd_source_fs);

	if (sidebar->expand_timeout_source > 0) {
          g_source_remove(sidebar->expand_timeout_source);
          sidebar->expand_timeout_source = 0;
        }

	if(sidebar->tree_view != NULL)
	{
	    g_signal_handlers_disconnect_by_data(sidebar->tree_view, sidebar);
	    gtk_tree_view_set_model(GTK_TREE_VIEW(sidebar->tree_view), NULL);
	    gtk_widget_destroy(GTK_WIDGET(sidebar->tree_view));
	    sidebar->tree_view = NULL;
	}

    if(sidebar->hidden_files_changed_id != 0) {
        g_signal_handler_disconnect (sidebar->window,
			                         sidebar->hidden_files_changed_id);
        sidebar->hidden_files_changed_id = 0;
    }

	g_clear_object (&sidebar->ui_manager);
	sidebar->ui_manager = NULL;

	free_drag_data (sidebar);

    g_clear_handle_id (&sidebar->actions_changed_idle_id, g_source_remove);

	if (sidebar->bookmarks_changed_id != 0) {
		g_signal_handler_disconnect (sidebar->bookmarks,
					     sidebar->bookmarks_changed_id);
		sidebar->bookmarks_changed_id = 0;
	}

    if (sidebar->actions_changed_id != 0) {
        g_signal_handler_disconnect (sidebar->action_manager,
                                     sidebar->actions_changed_id);
        sidebar->actions_changed_id = 0;
    }
    g_clear_object (&sidebar->action_manager);

    if (sidebar->update_places_on_idle_id != 0) {
        g_source_remove (sidebar->update_places_on_idle_id);
        sidebar->update_places_on_idle_id = 0;
    }
	if (sidebar->hidden_files_changed_id != 0 && sidebar->window) {
        g_signal_handler_disconnect(sidebar->window, sidebar->hidden_files_changed_id);
        sidebar->hidden_files_changed_id = 0;
    }

    g_clear_pointer (&sidebar->top_bookend_uri, g_free);
    g_clear_pointer (&sidebar->bottom_bookend_uri, g_free);

	if (sidebar->go_to_after_mount_slot) {
		g_object_remove_weak_pointer (G_OBJECT (sidebar->go_to_after_mount_slot),
					      (gpointer *) &sidebar->go_to_after_mount_slot);
		 g_clear_object (&sidebar->go_to_after_mount_slot);
	}

    g_signal_handlers_disconnect_by_func (nemo_window_state,
                                          breakpoint_changed_cb,
                                          sidebar);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
                                          reset_menu,
                                          sidebar);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      desktop_setting_changed_callback,
					      sidebar);

	g_signal_handlers_disconnect_by_func (nemo_desktop_preferences,
					      desktop_setting_changed_callback,
					      sidebar);

    g_signal_handlers_disconnect_by_func (cinnamon_privacy_preferences,
                          desktop_setting_changed_callback,
                          sidebar);

    g_signal_handlers_disconnect_by_func (xapp_favorites_get_default (),
                                          favorites_changed_cb,
                                          sidebar);

	if (sidebar->volume_monitor != NULL) {
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      volume_added_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      volume_removed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      volume_changed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      mount_added_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      mount_removed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      mount_changed_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      drive_disconnected_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      drive_connected_callback, sidebar);
		g_signal_handlers_disconnect_by_func (sidebar->volume_monitor,
						      drive_changed_callback, sidebar);

		g_clear_object (&sidebar->volume_monitor);
	}
	if(sidebar->selection_location) {
		g_free (sidebar->selection_location);
		sidebar->selection_location = NULL;
	}
	sidebar->window = NULL;
	G_OBJECT_CLASS (nemo_places_sidebar_parent_class)->dispose (object);
}

static void
nemo_places_tree_sidebar_finalize (GObject *object)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (object);

	if(sidebar) {
		if (sidebar->store_filter) g_clear_object(&sidebar->store_filter);
		if(sidebar->sort_model)	g_object_unref (sidebar->sort_model);
		if(sidebar->store) g_clear_object(&sidebar->store);
	}

	G_OBJECT_CLASS (nemo_places_sidebar_parent_class)->finalize (object);
}

static void
nemo_places_sidebar_class_init (NemoPlacesSidebarClass *class)
{
    GObjectClass *oclass = G_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    oclass->dispose = nemo_places_sidebar_dispose;
	oclass->finalize = nemo_places_tree_sidebar_finalize;

	widget_class->style_set = nemo_places_sidebar_style_set;
	widget_class->focus = nemo_places_sidebar_focus;

    gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &menu_icon_pixels, NULL);
    EJECT_ICON_SIZE_NOT_HOVERED = gtk_icon_size_register ("menu-icon-size-small",
                                                          menu_icon_pixels - EJECT_ICON_SIZE_REDUCTION,
                                                          menu_icon_pixels - EJECT_ICON_SIZE_REDUCTION);
	copied_files_atom = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);
}

static gboolean
update_places_on_idle_callback (NemoPlacesSidebar *sidebar)
{
    sidebar->update_places_on_idle_id = 0;

    update_places (sidebar);

    return FALSE;
}

static void
update_places_on_idle (NemoPlacesSidebar *sidebar)
{
    if (sidebar->update_places_on_idle_id != 0) {
        g_source_remove (sidebar->update_places_on_idle_id);
        sidebar->update_places_on_idle_id = 0;
    }

    sidebar->update_places_on_idle_id = g_idle_add_full (G_PRIORITY_LOW,
                                                         (GSourceFunc) update_places_on_idle_callback,
                                                         g_object_ref(sidebar),
														 (GDestroyNotify) g_object_unref);
}

static void
nemo_places_sidebar_set_parent_window (NemoPlacesSidebar *sidebar,
					   NemoWindow *window)
{
	NemoWindowSlot *slot;
    gint breakpoint;

	sidebar->window = window;


	g_signal_connect_object (window, "loading_uri",
					 G_CALLBACK (loading_uri_callback), sidebar, 0);

	if(sidebar->use_file_treeview) {
		sidebar->hidden_files_changed_id =
		                g_signal_connect_object (window, "hidden-files-mode-changed",
		                                         G_CALLBACK (hidden_files_mode_changed_callback), sidebar, 0);
	}
	slot = nemo_window_get_active_slot (window);

	sidebar->bookmarks = nemo_bookmark_list_get_default ();
	sidebar->selection_location = nemo_window_slot_get_current_uri (slot);

    breakpoint = g_settings_get_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT);

    if (breakpoint < 0) {     // Default gsettings value is -1 (which translates to 'not previously set')
        breakpoint = nemo_bookmark_list_length (sidebar->bookmarks);
        g_settings_set_int (nemo_window_state, NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT, breakpoint);
    }

    sidebar->bookmark_breakpoint = breakpoint;
    g_signal_connect_swapped (nemo_window_state, "changed::" NEMO_PREFERENCES_SIDEBAR_BOOKMARK_BREAKPOINT,
                              G_CALLBACK (breakpoint_changed_cb), sidebar);

	sidebar->bookmarks_changed_id =
		g_signal_connect_swapped (sidebar->bookmarks, "changed",
					  G_CALLBACK (update_places_on_idle),
					  sidebar);

    if (sidebar->volume_monitor != NULL) sidebar->volume_monitor = g_volume_monitor_get ();

	g_signal_connect_object (sidebar->volume_monitor, "volume_added",
				 G_CALLBACK (volume_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_removed",
				 G_CALLBACK (volume_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "volume_changed",
				 G_CALLBACK (volume_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "mount_changed",
				 G_CALLBACK (mount_changed_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_disconnected",
				 G_CALLBACK (drive_disconnected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_connected",
				 G_CALLBACK (drive_connected_callback), sidebar, 0);
	g_signal_connect_object (sidebar->volume_monitor, "drive_changed",
				 G_CALLBACK (drive_changed_callback), sidebar, 0);

	g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_ALWAYS_USE_BROWSER,
				 G_CALLBACK (reset_menu), sidebar);
//	update_places (sidebar);
}

static void
nemo_places_sidebar_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (widget);
	if(sidebar->tree_view)   update_places (sidebar);
}

GtkWidget *
nemo_places_sidebar_new (NemoWindow *window)
{
	NemoPlacesSidebar *sidebar;

	sidebar = g_object_new (NEMO_TYPE_PLACES_SIDEBAR, NULL);
	nemo_places_sidebar_set_parent_window (sidebar, window);

	return GTK_WIDGET (sidebar);
}


/* Drag and drop interfaces */

/* GtkTreeDragSource::row_draggable implementation for the shortcuts filter model */

static gboolean
nemo_shortcuts_model_row_draggable (GtkTreeDragSource *drag_source,
					GtkTreePath       *path)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	PlaceType place_type;
	SectionType section_type;

	model = GTK_TREE_MODEL (drag_source);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
			    -1);

	if (place_type != PLACES_HEADING &&
        (section_type == SECTION_XDG_BOOKMARKS || section_type == SECTION_BOOKMARKS))
		return TRUE;

	return FALSE;
}

static void
_nemo_shortcuts_model_class_init (NemoShortcutsModelClass *klass)
{

}

static void
_nemo_shortcuts_model_init (NemoShortcutsModel *model)
{
	model->sidebar = NULL;
}

static void
_nemo_shortcuts_model_drag_source_init (GtkTreeDragSourceIface *iface)
{
	iface->row_draggable = nemo_shortcuts_model_row_draggable;
}

static GtkTreeStore *
nemo_shortcuts_model_new (NemoPlacesSidebar *sidebar)
{
	NemoShortcutsModel *model;
	GType model_types[PLACES_SIDEBAR_COLUMN_COUNT] = {
		G_TYPE_STRING,      		// FM_TREE_MODEL_DISPLAY_NAME_COLUMN
		g_icon_get_type(),		// FM_TREE_MODEL_CLOSED_ICON_COLUMN
		g_icon_get_type(),		// FM_TREE_MODEL_OPEN_ICON_COLUMN
		PANGO_TYPE_STYLE,		// FM_TREE_MODEL_FONT_STYLE_COLUMN
		G_TYPE_INT,				// FM_TREE_MODEL_TEXT_WEIGHT_COLUMN
        G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_ROW_TYPE
		G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_URI
		G_TYPE_DRIVE,			// PLACES_SIDEBAR_COLUMN_DRIVE
		G_TYPE_VOLUME,			// PLACES_SIDEBAR_COLUMN_VOLUME
		G_TYPE_MOUNT,			// PLACES_SIDEBAR_COLUMN_MOUNT
		G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_INDEX
		G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_EJECT
		G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_NO_EJECT
		G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_BOOKMARK
		G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_TOOLTIP
		G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_EJECT_ICON
        G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_EJECT_ICON_SIZE
		G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_SECTION_TYPE
		G_TYPE_STRING,			// PLACES_SIDEBAR_COLUMN_HEADING_TEXT
        G_TYPE_INT,				// PLACES_SIDEBAR_COLUMN_DF_PERCENT
		G_TYPE_BOOLEAN,			// PLACES_SIDEBAR_COLUMN_SHOW_DF
		G_TYPE_STRING			// PLACES_SIDEBAR_COLUMN_TREE_NAME
	};

	model = g_object_new (NEMO_TYPE_SHORTCUTS_MODEL, NULL);
	model->sidebar = sidebar;

	gtk_tree_store_set_column_types (GTK_TREE_STORE (model),
					 PLACES_SIDEBAR_COLUMN_COUNT,
					 model_types);

	return GTK_TREE_STORE (model);
}

static gboolean
nemo_places_sidebar_row_draggable (GtkTreeDragSource *drag_source,
								  GtkTreePath       *path,
								  gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	PlaceType place_type;
	SectionType section_type;

	model = GTK_TREE_MODEL (drag_source);

	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_model_get (model, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type,
			    -1);

	if (place_type != PLACES_HEADING &&
        (section_type == SECTION_XDG_BOOKMARKS || section_type == SECTION_BOOKMARKS))
		return TRUE;

	return FALSE;
}
