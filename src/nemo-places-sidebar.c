/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Cosimo Cecchi <cosimoc@gnome.org>
 *
 */
 
#include <config.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-bookmark.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-trash-monitor.h>
#include <libnemo-private/nemo-icon-names.h>

#include <eel/eel-debug.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>

#include "nemo-application.h"
#include "nemo-bookmark-list.h"
#include "nemo-places-sidebar.h"
#include "nemo-properties-window.h"
#include "nemo-window.h"
#include "nemo-window-slot.h"

#define DEBUG_FLAG NEMO_DEBUG_PLACES
#include <libnemo-private/nemo-debug.h>

#define EJECT_BUTTON_XPAD 6
#define ICON_CELL_XPAD 6

typedef struct {
	GtkScrolledWindow  parent;
	GtkTreeView        *tree_view;
	GtkCellRenderer    *eject_icon_cell_renderer;
	char 	           *uri;
	GtkListStore       *store;
	NemoWindow *window;
	NemoBookmarkList *bookmarks;
	GVolumeMonitor *volume_monitor;

	gboolean devices_header_added;
	gboolean bookmarks_header_added;

	/* DnD */
	GList     *drag_list;
	gboolean  drag_data_received;
	int       drag_data_info;
	gboolean  drop_occured;

	GtkWidget *popup_menu;
	GtkWidget *popup_menu_open_in_new_tab_item;
	GtkWidget *popup_menu_add_shortcut_item;
	GtkWidget *popup_menu_remove_item;
	GtkWidget *popup_menu_rename_item;
	GtkWidget *popup_menu_separator_item;
	GtkWidget *popup_menu_mount_item;
	GtkWidget *popup_menu_unmount_item;
	GtkWidget *popup_menu_eject_item;
	GtkWidget *popup_menu_rescan_item;
	GtkWidget *popup_menu_empty_trash_item;
	GtkWidget *popup_menu_start_item;
	GtkWidget *popup_menu_stop_item;
	GtkWidget *popup_menu_properties_separator_item;
	GtkWidget *popup_menu_properties_item;

	/* volume mounting - delayed open process */
	gboolean mounting;
	NemoWindowSlot *go_to_after_mount_slot;
	NemoWindowOpenFlags go_to_after_mount_flags;

	GtkTreePath *eject_highlight_path;

	guint bookmarks_changed_id;
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
	PLACES_SIDEBAR_COLUMN_ROW_TYPE,
	PLACES_SIDEBAR_COLUMN_URI,
	PLACES_SIDEBAR_COLUMN_DRIVE,
	PLACES_SIDEBAR_COLUMN_VOLUME,
	PLACES_SIDEBAR_COLUMN_MOUNT,
	PLACES_SIDEBAR_COLUMN_NAME,
	PLACES_SIDEBAR_COLUMN_ICON,
	PLACES_SIDEBAR_COLUMN_INDEX,
	PLACES_SIDEBAR_COLUMN_EJECT,
	PLACES_SIDEBAR_COLUMN_NO_EJECT,
	PLACES_SIDEBAR_COLUMN_BOOKMARK,
	PLACES_SIDEBAR_COLUMN_TOOLTIP,
	PLACES_SIDEBAR_COLUMN_EJECT_ICON,
	PLACES_SIDEBAR_COLUMN_SECTION_TYPE,
	PLACES_SIDEBAR_COLUMN_HEADING_TEXT,

	PLACES_SIDEBAR_COLUMN_COUNT
};

typedef enum {
	PLACES_BUILT_IN,
	PLACES_XDG_DIR,
	PLACES_MOUNTED_VOLUME,
	PLACES_BOOKMARK,
	PLACES_HEADING,
} PlaceType;

typedef enum {
	SECTION_DEVICES,
	SECTION_BOOKMARKS,
	SECTION_COMPUTER,
	SECTION_NETWORK,
} SectionType;

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

static void bookmarks_check_popup_sensitivity          (NemoPlacesSidebar *sidebar);

/* Identifiers for target types */
enum {
  GTK_TREE_MODEL_ROW,
  TEXT_URI_LIST
};

/* Target types for dragging from the shortcuts list */
static const GtkTargetEntry nemo_shortcuts_source_targets[] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW }
};

/* Target types for dropping into the shortcuts list */
static const GtkTargetEntry nemo_shortcuts_drop_targets [] = {
	{ "GTK_TREE_MODEL_ROW", GTK_TARGET_SAME_WIDGET, GTK_TREE_MODEL_ROW },
	{ "text/uri-list", 0, TEXT_URI_LIST }
};

/* Drag and drop interface declarations */
typedef struct {
  GtkListStore parent;

  NemoPlacesSidebar *sidebar;
} NemoShortcutsModel;

typedef struct {
  GtkListStoreClass parent_class;
} NemoShortcutsModelClass;

#define NEMO_TYPE_SHORTCUTS_MODEL (_nemo_shortcuts_model_get_type ())
#define NEMO_SHORTCUTS_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_SHORTCUTS_MODEL_TYPE, NemoShortcutsModel))

GType _nemo_shortcuts_model_get_type (void);
static void _nemo_shortcuts_model_drag_source_init (GtkTreeDragSourceIface *iface);
G_DEFINE_TYPE_WITH_CODE (NemoShortcutsModel, _nemo_shortcuts_model, GTK_TYPE_LIST_STORE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_DRAG_SOURCE,
						_nemo_shortcuts_model_drag_source_init));
static GtkListStore *nemo_shortcuts_model_new (NemoPlacesSidebar *sidebar);

G_DEFINE_TYPE (NemoPlacesSidebar, nemo_places_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static GdkPixbuf *
get_eject_icon (NemoPlacesSidebar *sidebar,
		gboolean highlighted)
{
	GdkPixbuf *eject;
	GtkIconInfo *icon_info;
	GIcon *icon;
	int icon_size;
	GtkIconTheme *icon_theme;
	GtkStyleContext *style;
	GtkStateFlags state;

	icon_theme = gtk_icon_theme_get_default ();
	icon_size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	icon = g_themed_icon_new_with_default_fallbacks ("media-eject");
	icon_info = gtk_icon_theme_lookup_by_gicon (icon_theme, icon, icon_size, 0);

	style = gtk_widget_get_style_context (GTK_WIDGET (sidebar));
	gtk_style_context_save (style);

	if (icon_info != NULL) {
		state = gtk_widget_get_state_flags (GTK_WIDGET (sidebar));
		gtk_style_context_add_class (style, GTK_STYLE_CLASS_IMAGE);

		if (highlighted) {
			state |= GTK_STATE_FLAG_PRELIGHT;
		}

		gtk_style_context_set_state (style, state);

		eject = gtk_icon_info_load_symbolic_for_context (icon_info,
								 style,
								 NULL,
								 NULL);

		gtk_icon_info_free (icon_info);
	} else {
		GtkIconSet *icon_set;

		gtk_style_context_set_state (style, GTK_STATE_FLAG_NORMAL);
		icon_set = gtk_style_context_lookup_icon_set (style, GTK_STOCK_MISSING_IMAGE);
		eject = gtk_icon_set_render_icon_pixbuf (icon_set, style, GTK_ICON_SIZE_MENU);
	}

	gtk_style_context_restore (style);
	g_object_unref (icon);

	return eject;
}

static gboolean
should_show_desktop (void)
{
	return g_settings_get_boolean (gnome_background_preferences, NEMO_PREFERENCES_SHOW_DESKTOP) &&
	       !g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR);
}

static gboolean
is_built_in_bookmark (NemoFile *file)
{
	gboolean built_in;
	gint idx;

	if (nemo_file_is_home (file)) {
		return TRUE;
	}

	if (nemo_file_is_desktop_directory (file) && !should_show_desktop ()) {
		return FALSE;
	}

	built_in = FALSE;

	for (idx = 0; idx < G_USER_N_DIRECTORIES; idx++) {
		/* PUBLIC_SHARE and TEMPLATES are not in our built-in list */
		if (nemo_file_is_user_special_directory (file, idx)) {
			if (idx != G_USER_DIRECTORY_PUBLIC_SHARE &&  idx != G_USER_DIRECTORY_TEMPLATES) {
				built_in = TRUE;
			}

			break;
		}
	}

	return built_in;
}

static GtkTreeIter
add_heading (NemoPlacesSidebar *sidebar,
	     SectionType section_type,
	     const gchar *title)
{
	GtkTreeIter iter;

	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, PLACES_HEADING,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,	
			    PLACES_SIDEBAR_COLUMN_HEADING_TEXT, title,
			    PLACES_SIDEBAR_COLUMN_EJECT, FALSE,
			    PLACES_SIDEBAR_COLUMN_NO_EJECT, TRUE,
			    -1);

	return iter;
}

static void
check_heading_for_section (NemoPlacesSidebar *sidebar,
			   SectionType section_type)
{
	switch (section_type) {
	case SECTION_DEVICES:
		if (!sidebar->devices_header_added) {
			add_heading (sidebar, SECTION_DEVICES,
				     _("Devices"));
			sidebar->devices_header_added = TRUE;
		}

		break;
	case SECTION_BOOKMARKS:
		if (!sidebar->bookmarks_header_added) {
			add_heading (sidebar, SECTION_BOOKMARKS,
				     _("Bookmarks"));
			sidebar->bookmarks_header_added = TRUE;
		}

		break;
	default:
		break;
	}
}

static void
add_place (NemoPlacesSidebar *sidebar,
	   PlaceType place_type,
	   SectionType section_type,
	   const char *name,
	   GIcon *icon,
	   const char *uri,
	   GDrive *drive,
	   GVolume *volume,
	   GMount *mount,
	   const int index,
	   const char *tooltip)
{
	GdkPixbuf            *pixbuf;
	GtkTreeIter           iter;
	GdkPixbuf	     *eject;
	NemoIconInfo *icon_info;
	int icon_size;
	gboolean show_eject, show_unmount;
	gboolean show_eject_button;

	check_heading_for_section (sidebar, section_type);

	icon_size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);
	icon_info = nemo_icon_info_lookup (icon, icon_size);

	pixbuf = nemo_icon_info_get_pixbuf_at_size (icon_info, icon_size);
	g_object_unref (icon_info);

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

	if (show_eject_button) {
		eject = get_eject_icon (sidebar, FALSE);
	} else {
		eject = NULL;
	}

	gtk_list_store_append (sidebar->store, &iter);
	gtk_list_store_set (sidebar->store, &iter,
			    PLACES_SIDEBAR_COLUMN_ICON, pixbuf,
			    PLACES_SIDEBAR_COLUMN_NAME, name,
			    PLACES_SIDEBAR_COLUMN_URI, uri,
			    PLACES_SIDEBAR_COLUMN_DRIVE, drive,
			    PLACES_SIDEBAR_COLUMN_VOLUME, volume,
			    PLACES_SIDEBAR_COLUMN_MOUNT, mount,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, place_type,
			    PLACES_SIDEBAR_COLUMN_INDEX, index,
			    PLACES_SIDEBAR_COLUMN_EJECT, show_eject_button,
			    PLACES_SIDEBAR_COLUMN_NO_EJECT, !show_eject_button,
			    PLACES_SIDEBAR_COLUMN_BOOKMARK, place_type != PLACES_BOOKMARK,
			    PLACES_SIDEBAR_COLUMN_TOOLTIP, tooltip,
			    PLACES_SIDEBAR_COLUMN_EJECT_ICON, eject,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, section_type,
			    -1);

	if (pixbuf != NULL) {
		g_object_unref (pixbuf);
	}
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
	gchar *uri;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_URI, &uri,
			    -1);

	if (g_strcmp0 (uri, data->last_uri) == 0 ||
	    g_strcmp0 (uri, data->location) == 0) {
		data->path = gtk_tree_path_copy (path);
	}

	g_free (uri);

	return (data->path != NULL);
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

	gtk_tree_model_foreach (GTK_TREE_MODEL (sidebar->store),
				restore_selection_foreach, &data);

	if (data.path != NULL) {
		selection = gtk_tree_view_get_selection (sidebar->tree_view);
		gtk_tree_selection_select_path (selection, data.path);
		gtk_tree_path_free (data.path);
	}
}

static void
update_places (NemoPlacesSidebar *sidebar)
{
	NemoBookmark *bookmark;
	GtkTreeSelection *selection;
	GtkTreeIter last_iter;
	GtkTreeModel *model;
	GVolumeMonitor *volume_monitor;
	GList *mounts, *l, *ll;
	GMount *mount;
	GList *drives;
	GDrive *drive;
	GList *volumes;
	GVolume *volume;
	int bookmark_count, index;
	char *location, *mount_uri, *name, *desktop_path, *last_uri, *identifier;
	const gchar *path, *bookmark_name;
	GIcon *icon;
	GFile *root;
	NemoWindowSlot *slot;
	char *tooltip;
	GList *network_mounts, *network_volumes;
	NemoFile *file;

	DEBUG ("Updating places sidebar");

	model = NULL;
	last_uri = NULL;

	selection = gtk_tree_view_get_selection (sidebar->tree_view);
	if (gtk_tree_selection_get_selected (selection, &model, &last_iter)) {
		gtk_tree_model_get (model,
				    &last_iter,
				    PLACES_SIDEBAR_COLUMN_URI, &last_uri, -1);
	}
	gtk_list_store_clear (sidebar->store);

	sidebar->devices_header_added = FALSE;
	sidebar->bookmarks_header_added = FALSE;

	slot = nemo_window_get_active_slot (sidebar->window);
	location = nemo_window_slot_get_current_uri (slot);

	network_mounts = network_volumes = NULL;
	volume_monitor = sidebar->volume_monitor;

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
					/* Show mounted volume in the sidebar */
					icon = g_mount_get_icon (mount);
					root = g_mount_get_default_location (mount);
					mount_uri = g_file_get_uri (root);
					name = g_mount_get_name (mount);
					tooltip = g_file_get_parse_name (root);

					add_place (sidebar, PLACES_MOUNTED_VOLUME,
						   SECTION_DEVICES,
						   name, icon, mount_uri,
						   drive, volume, mount, 0, tooltip);
					g_object_unref (root);
					g_object_unref (mount);
					g_object_unref (icon);
					g_free (tooltip);
					g_free (name);
					g_free (mount_uri);
				} else {
					/* Do show the unmounted volumes in the sidebar;
					 * this is so the user can mount it (in case automounting
					 * is off).
					 *
					 * Also, even if automounting is enabled, this gives a visual
					 * cue that the user should remember to yank out the media if
					 * he just unmounted it.
					 */
					icon = g_volume_get_icon (volume);
					name = g_volume_get_name (volume);
					tooltip = g_strdup_printf (_("Mount and open %s"), name);

					add_place (sidebar, PLACES_MOUNTED_VOLUME,
						   SECTION_DEVICES,
						   name, icon, NULL,
						   drive, volume, NULL, 0, tooltip);
					g_object_unref (icon);
					g_free (name);
					g_free (tooltip);
				}
				g_object_unref (volume);
			}
			g_list_free (volumes);
		} else {
			if (g_drive_is_media_removable (drive) && !g_drive_is_media_check_automatic (drive)) {
				/* If the drive has no mountable volumes and we cannot detect media change.. we
				 * display the drive in the sidebar so the user can manually poll the drive by
				 * right clicking and selecting "Rescan..."
				 *
				 * This is mainly for drives like floppies where media detection doesn't
				 * work.. but it's also for human beings who like to turn off media detection
				 * in the OS to save battery juice.
				 */
				icon = g_drive_get_icon (drive);
				name = g_drive_get_name (drive);
				tooltip = g_strdup_printf (_("Mount and open %s"), name);

				add_place (sidebar, PLACES_BUILT_IN,
					   SECTION_DEVICES,
					   name, icon, NULL,
					   drive, NULL, NULL, 0, tooltip);
				g_object_unref (icon);
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
			icon = g_mount_get_icon (mount);
			root = g_mount_get_default_location (mount);
			mount_uri = g_file_get_uri (root);
			tooltip = g_file_get_parse_name (root);
			g_object_unref (root);
			name = g_mount_get_name (mount);
			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_DEVICES,
				   name, icon, mount_uri,
				   NULL, volume, mount, 0, tooltip);
			g_object_unref (mount);
			g_object_unref (icon);
			g_free (name);
			g_free (tooltip);
			g_free (mount_uri);
		} else {
			/* see comment above in why we add an icon for an unmounted mountable volume */
			icon = g_volume_get_icon (volume);
			name = g_volume_get_name (volume);
			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_DEVICES,
				   name, icon, NULL,
				   NULL, volume, NULL, 0, name);
			g_object_unref (icon);
			g_free (name);
		}
		g_object_unref (volume);
	}
	g_list_free (volumes);
	
	/* add bookmarks */
	bookmark_count = nemo_bookmark_list_length (sidebar->bookmarks);

	for (index = 0; index < bookmark_count; ++index) {
		bookmark = nemo_bookmark_list_item_at (sidebar->bookmarks, index);

		if (nemo_bookmark_uri_known_not_to_exist (bookmark)) {
			continue;
		}

		root = nemo_bookmark_get_location (bookmark);
		file = nemo_file_get (root);

		if (is_built_in_bookmark (file)) {
			g_object_unref (root);
			nemo_file_unref (file);
			continue;
		}
		nemo_file_unref (file);

		bookmark_name = nemo_bookmark_get_name (bookmark);
		icon = nemo_bookmark_get_icon (bookmark);
		mount_uri = nemo_bookmark_get_uri (bookmark);
		tooltip = g_file_get_parse_name (root);

		add_place (sidebar, PLACES_BOOKMARK,
			   SECTION_BOOKMARKS,
			   bookmark_name, icon, mount_uri,
			   NULL, NULL, NULL, index,
			   tooltip);
		g_object_unref (root);
		g_object_unref (icon);
		g_free (mount_uri);
		g_free (tooltip);
	}

	add_heading (sidebar, SECTION_COMPUTER,
		     _("Computer"));

	/* add built in bookmarks */

	/* home folder */
	mount_uri = nemo_get_home_directory_uri ();
	icon = g_themed_icon_new (NEMO_ICON_HOME);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_COMPUTER,
		   _("Home"), icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Open your personal folder"));
	g_object_unref (icon);
	g_free (mount_uri);

	if (should_show_desktop ()) {
		/* desktop */
		desktop_path = nemo_get_desktop_directory ();
		mount_uri = g_filename_to_uri (desktop_path, NULL, NULL);
		icon = g_themed_icon_new (NEMO_ICON_DESKTOP);
		add_place (sidebar, PLACES_BUILT_IN,
			   SECTION_COMPUTER,
			   _("Desktop"), icon,
			   mount_uri, NULL, NULL, NULL, 0,
			   _("Open the contents of your desktop in a folder"));
		g_object_unref (icon);
		g_free (mount_uri);
		g_free (desktop_path);
	}

	
	/* XDG directories */
	for (index = 0; index < G_USER_N_DIRECTORIES; index++) {

		if (index == G_USER_DIRECTORY_DESKTOP ||
		    index == G_USER_DIRECTORY_TEMPLATES ||
		    index == G_USER_DIRECTORY_PUBLIC_SHARE) {
			continue;
		}

		path = g_get_user_special_dir (index);

		/* xdg resets special dirs to the home directory in case
		 * it's not finiding what it expects. We don't want the home
		 * to be added multiple times in that weird configuration.
		 */
		if (!path || g_strcmp0 (path, g_get_home_dir ()) == 0) {
			continue;
		}

		root = g_file_new_for_path (path);
		name = g_file_get_basename (root);
		icon = nemo_user_special_directory_get_gicon (index);
		mount_uri = g_file_get_uri (root);
		tooltip = g_file_get_parse_name (root);

		add_place (sidebar, PLACES_XDG_DIR,
			   SECTION_COMPUTER,
			   name, icon, mount_uri,
			   NULL, NULL, NULL, 0,
			   tooltip);
		g_free (name);
		g_object_unref (root);
		g_object_unref (icon);
		g_free (mount_uri);
		g_free (tooltip);
	}

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
			network_mounts = g_list_prepend (network_mounts, mount);
			continue;
		}

		icon = g_mount_get_icon (mount);
		mount_uri = g_file_get_uri (root);
		name = g_mount_get_name (mount);
		tooltip = g_file_get_parse_name (root);
		add_place (sidebar, PLACES_MOUNTED_VOLUME,
			   SECTION_COMPUTER,
			   name, icon, mount_uri,
			   NULL, NULL, mount, 0, tooltip);
		g_object_unref (root);
		g_object_unref (mount);
		g_object_unref (icon);
		g_free (name);
		g_free (mount_uri);
		g_free (tooltip);
	}
	g_list_free (mounts);

	/* file system root */
 	mount_uri = "file:///"; /* No need to strdup */
	icon = g_themed_icon_new (NEMO_ICON_FILESYSTEM);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_COMPUTER,
		   _("File System"), icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Open the contents of the File System"));
	g_object_unref (icon);

	mount_uri = "trash:///"; /* No need to strdup */
	icon = nemo_trash_monitor_get_icon ();
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_COMPUTER,
		   _("Trash"), icon, mount_uri,
		   NULL, NULL, NULL, 0,
		   _("Open the trash"));
	g_object_unref (icon);

	/* network */
	add_heading (sidebar, SECTION_NETWORK,
		     _("Network"));

	network_volumes = g_list_reverse (network_volumes);
	for (l = network_volumes; l != NULL; l = l->next) {
		volume = l->data;
		mount = g_volume_get_mount (volume);

		if (mount != NULL) {
			network_mounts = g_list_prepend (network_mounts, mount);
			continue;
		} else {
			icon = g_volume_get_icon (volume);
			name = g_volume_get_name (volume);
			tooltip = g_strdup_printf (_("Mount and open %s"), name);

			add_place (sidebar, PLACES_MOUNTED_VOLUME,
				   SECTION_NETWORK,
				   name, icon, NULL,
				   NULL, volume, NULL, 0, tooltip);
			g_object_unref (icon);
			g_free (name);
			g_free (tooltip);
		}
	}

	g_list_free_full (network_volumes, g_object_unref);

	network_mounts = g_list_reverse (network_mounts);
	for (l = network_mounts; l != NULL; l = l->next) {
		mount = l->data;
		root = g_mount_get_default_location (mount);
		icon = g_mount_get_icon (mount);
		mount_uri = g_file_get_uri (root);
		name = g_mount_get_name (mount);
		tooltip = g_file_get_parse_name (root);
		add_place (sidebar, PLACES_MOUNTED_VOLUME,
			   SECTION_NETWORK,
			   name, icon, mount_uri,
			   NULL, NULL, mount, 0, tooltip);
		g_object_unref (root);
		g_object_unref (icon);
		g_free (name);
		g_free (mount_uri);
		g_free (tooltip);
	}

	g_list_free_full (network_mounts, g_object_unref);

	/* network:// */
 	mount_uri = "network:///"; /* No need to strdup */
	icon = g_themed_icon_new (NEMO_ICON_NETWORK);
	add_place (sidebar, PLACES_BUILT_IN,
		   SECTION_NETWORK,
		   _("Browse Network"), icon,
		   mount_uri, NULL, NULL, NULL, 0,
		   _("Browse the contents of the network"));
	g_object_unref (icon);

	/* restore selection */
	sidebar_update_restore_selection (sidebar, location, last_uri);

	g_free (location);
	g_free (last_uri);
}

static void
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_added_callback (GVolumeMonitor *volume_monitor,
		       GVolume *volume,
		       NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_removed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
volume_changed_callback (GVolumeMonitor *volume_monitor,
			 GVolume *volume,
			 NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_disconnected_callback (GVolumeMonitor *volume_monitor,
			     GDrive         *drive,
			     NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_connected_callback (GVolumeMonitor *volume_monitor,
			  GDrive         *drive,
			  NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static void
drive_changed_callback (GVolumeMonitor *volume_monitor,
			GDrive         *drive,
			NemoPlacesSidebar *sidebar)
{
	update_places (sidebar);
}

static gboolean
over_eject_button (NemoPlacesSidebar *sidebar,
		   gint x,
		   gint y,
		   GtkTreePath **path)
{
	GtkTreeViewColumn *column;
	int width, x_offset, hseparator;
	int eject_button_size;
	gboolean show_eject;
	GtkTreeIter iter;
	GtkTreeModel *model;

	*path = NULL;
	model = gtk_tree_view_get_model (sidebar->tree_view);

	if (gtk_tree_view_get_path_at_pos (sidebar->tree_view,
					   x, y,
					   path, &column, NULL, NULL)) {

		gtk_tree_model_get_iter (model, &iter, *path);
		gtk_tree_model_get (model, &iter,
				    PLACES_SIDEBAR_COLUMN_EJECT, &show_eject,
				    -1);

		if (!show_eject) {
			goto out;
		}


		gtk_widget_style_get (GTK_WIDGET (sidebar->tree_view),
				      "horizontal-separator", &hseparator,
				      NULL);

		/* Reload cell attributes for this particular row */
		gtk_tree_view_column_cell_set_cell_data (column,
							 model, &iter, FALSE, FALSE);

		gtk_tree_view_column_cell_get_position (column,
							sidebar->eject_icon_cell_renderer,
							&x_offset, &width);

		eject_button_size = nemo_get_icon_size_for_stock_size (GTK_ICON_SIZE_MENU);

		/* This is kinda weird, but we have to do it to workaround gtk+ expanding
		 * the eject cell renderer (even thought we told it not to) and we then
		 * had to set it right-aligned */
		x_offset += width - hseparator - EJECT_BUTTON_XPAD - eject_button_size;

		if (x - x_offset >= 0 &&
		    x - x_offset <= eject_button_size) {
			return TRUE;
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
	GdkEvent *event = gtk_get_current_event ();
	GdkEventButton *button_event = (GdkEventButton *) event;

	if ((event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) &&
	     over_eject_button (sidebar, button_event->x, button_event->y, path)) {
		return TRUE;
	}

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
	GtkTreeIter 	 iter;
	gboolean 	 valid;
	char  		 *uri;

        if (strcmp (sidebar->uri, location) != 0) {
		g_free (sidebar->uri);
                sidebar->uri = g_strdup (location);
  
		/* set selection if any place matches location */
		selection = gtk_tree_view_get_selection (sidebar->tree_view);
		gtk_tree_selection_unselect_all (selection);
  		valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store),
						       &iter);

		while (valid) {
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter, 
		 		       	    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
			if (uri != NULL) {
				if (strcmp (uri, location) == 0) {
					g_free (uri);
					gtk_tree_selection_select_iter (selection, &iter);
					break;
				}
				g_free (uri);
			}
        	 	valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (sidebar->store),
							  &iter);
		}
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
			    -1);

	if (place_type == PLACES_HEADING && section_type != SECTION_BOOKMARKS) {
		/* never drop on headings, but special case the bookmarks heading,
		 * so we can drop bookmarks in between it and the first item.
		 */

		gtk_tree_path_free (*path);
		*path = NULL;
		
		return FALSE;
	}

	if (section_type != SECTION_BOOKMARKS &&
	    sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
		/* don't allow dropping bookmarks into non-bookmark areas */

		gtk_tree_path_free (*path);
		*path = NULL;

		return FALSE;
	}

	if (section_type == SECTION_BOOKMARKS) {
		*pos = GTK_TREE_VIEW_DROP_AFTER;
	} else {
		/* non-bookmark shortcuts can only be dragged into */
		*pos = GTK_TREE_VIEW_DROP_INTO_OR_BEFORE;
	}

	if (*pos != GTK_TREE_VIEW_DROP_BEFORE &&
	    sidebar->drag_data_received &&
	    sidebar->drag_data_info == GTK_TREE_MODEL_ROW) {
		/* bookmark rows are never dragged into other bookmark rows */
		*pos = GTK_TREE_VIEW_DROP_AFTER;
	}

	return TRUE;
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

	if (!sidebar->drag_data_received) {
		if (!get_drag_data (tree_view, context, time)) {
			return FALSE;
		}
	}

	path = NULL;
	res = compute_drop_position (tree_view, x, y, &path, &pos, sidebar);

	if (!res) {
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
			gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
						 &iter, path);
			gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
					    &iter,
					    PLACES_SIDEBAR_COLUMN_URI, &uri,
					    -1);
			nemo_drag_default_drop_action_for_icons (context, uri,
								     sidebar->drag_list,
								     &action);
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
		     GtkSelectionData      *selection_data,
		     int                    position)
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

		bookmark = nemo_bookmark_new (location, NULL, NULL);

		if (!nemo_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
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

/* Reorders the selected bookmark to the specified position */
static void
reorder_bookmarks (NemoPlacesSidebar *sidebar,
		   int                   new_position)
{
	GtkTreeIter iter;
	PlaceType type; 
	int old_position;

	/* Get the selected path */
	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    PLACES_SIDEBAR_COLUMN_INDEX, &old_position,
			    -1);

	if (type != PLACES_BOOKMARK ||
	    old_position < 0 ||
	    old_position >= nemo_bookmark_list_length (sidebar->bookmarks)) {
		return;
	}

	nemo_bookmark_list_move_item (sidebar->bookmarks, old_position,
					  new_position);
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
			sidebar->drag_list = build_selection_list (gtk_selection_data_get_data (selection_data));
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

		if (section_type != SECTION_BOOKMARKS) {
			goto out;
		}

		if (tree_pos == GTK_TREE_VIEW_DROP_AFTER && place_type != PLACES_HEADING) {
			/* heading already has position 0 */
			position++;
		}

		switch (info) {
		case TEXT_URI_LIST:
			bookmarks_drop_uris (sidebar, selection_data, position);
			success = TRUE;
			break;
		case GTK_TREE_MODEL_ROW:
			reorder_bookmarks (sidebar, position);
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

			switch (info) {
			case TEXT_URI_LIST:
				selection_list = build_selection_list (gtk_selection_data_get_data (selection_data));
				uris = uri_list_from_selection (selection_list);
				nemo_file_operations_copy_move (uris, NULL, drop_uri,
								    real_action, GTK_WIDGET (tree_view),
								    NULL, NULL);
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

/* Callback used when the file list's popup menu is detached */
static void
bookmarks_popup_menu_detach_cb (GtkWidget *attach_widget,
				GtkMenu   *menu)
{
	NemoPlacesSidebar *sidebar;
	
	sidebar = NEMO_PLACES_SIDEBAR (attach_widget);
	g_assert (NEMO_IS_PLACES_SIDEBAR (sidebar));
	
	sidebar->popup_menu = NULL;
	sidebar->popup_menu_add_shortcut_item = NULL;
	sidebar->popup_menu_remove_item = NULL;
	sidebar->popup_menu_rename_item = NULL;
	sidebar->popup_menu_separator_item = NULL;
	sidebar->popup_menu_mount_item = NULL;
	sidebar->popup_menu_unmount_item = NULL;
	sidebar->popup_menu_eject_item = NULL;
	sidebar->popup_menu_rescan_item = NULL;
	sidebar->popup_menu_start_item = NULL;
	sidebar->popup_menu_stop_item = NULL;
	sidebar->popup_menu_empty_trash_item = NULL;
	sidebar->popup_menu_properties_separator_item = NULL;
	sidebar->popup_menu_properties_item = NULL;
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
		if (g_drive_is_media_removable (drive) &&
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
bookmarks_check_popup_sensitivity (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	PlaceType type; 
	GDrive *drive = NULL;
	GVolume *volume = NULL;
	GMount *mount = NULL;
	GFile *location;
	NemoDirectory *directory;
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

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
 				    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
				    PLACES_SIDEBAR_COLUMN_URI, &uri,
				    -1);
	}

	gtk_widget_set_visible (sidebar->popup_menu_add_shortcut_item, (type == PLACES_MOUNTED_VOLUME));

	gtk_widget_set_sensitive (sidebar->popup_menu_remove_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_rename_item, (type == PLACES_BOOKMARK));
	gtk_widget_set_sensitive (sidebar->popup_menu_empty_trash_item, !nemo_trash_monitor_is_empty ());

 	check_visibility (mount, volume, drive,
 			  &show_mount, &show_unmount, &show_eject, &show_rescan, &show_start, &show_stop);

	/* We actually want both eject and unmount since eject will unmount all volumes. 
	 * TODO: hide unmount if the drive only has a single mountable volume 
	 */

	show_empty_trash = (uri != NULL) &&
			   (!strcmp (uri, "trash:///"));

	/* Only show properties for local mounts */
	show_properties = (mount != NULL);
	if (mount != NULL) {
		location = g_mount_get_default_location (mount);
		directory = nemo_directory_get (location);

		show_properties = nemo_directory_is_local (directory);

		nemo_directory_unref (directory);
		g_object_unref (location);
	}

	gtk_widget_set_visible (sidebar->popup_menu_separator_item,
		      show_mount || show_unmount || show_eject || show_empty_trash);
	gtk_widget_set_visible (sidebar->popup_menu_mount_item, show_mount);
	gtk_widget_set_visible (sidebar->popup_menu_unmount_item, show_unmount);
	gtk_widget_set_visible (sidebar->popup_menu_eject_item, show_eject);
	gtk_widget_set_visible (sidebar->popup_menu_rescan_item, show_rescan);
	gtk_widget_set_visible (sidebar->popup_menu_start_item, show_start);
	gtk_widget_set_visible (sidebar->popup_menu_stop_item, show_stop);
	gtk_widget_set_visible (sidebar->popup_menu_empty_trash_item, show_empty_trash);
	gtk_widget_set_visible (sidebar->popup_menu_properties_separator_item, show_properties);
	gtk_widget_set_visible (sidebar->popup_menu_properties_item, show_properties);

	/* Adjust start/stop items to reflect the type of the drive */
	gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start"));
	gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop"));
	if ((show_start || show_stop) && drive != NULL) {
		switch (g_drive_get_start_stop_type (drive)) {
		case G_DRIVE_START_STOP_TYPE_SHUTDOWN:
			/* start() for type G_DRIVE_START_STOP_TYPE_SHUTDOWN is normally not used */
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Power On"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Safely Remove Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_NETWORK:
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Connect Drive"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Disconnect Drive"));
			break;
		case G_DRIVE_START_STOP_TYPE_MULTIDISK:
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Start Multi-disk Device"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Stop Multi-disk Device"));
			break;
		case G_DRIVE_START_STOP_TYPE_PASSWORD:
			/* stop() for type G_DRIVE_START_STOP_TYPE_PASSWORD is normally not used */
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_start_item), _("_Unlock Drive"));
			gtk_menu_item_set_label (GTK_MENU_ITEM (sidebar->popup_menu_stop_item), _("_Lock Drive"));
			break;

		default:
		case G_DRIVE_START_STOP_TYPE_UNKNOWN:
			/* uses defaults set above */
			break;
		}
	}


	g_free (uri);
}

/* Callback used when the selection in the shortcuts tree changes */
static void
bookmarks_selection_changed_cb (GtkTreeSelection      *selection,
				NemoPlacesSidebar *sidebar)
{
	bookmarks_check_popup_sensitivity (sidebar);
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
								    sidebar->go_to_after_mount_flags, NULL);
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

	
	eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));
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

	gtk_tree_model_get (model, iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

	if (uri != NULL) {
		DEBUG ("Activating bookmark %s", uri);

		location = g_file_new_for_uri (uri);
		/* Navigate to the clicked location */
		if ((flags & NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW) == 0) {
			slot = nemo_window_get_active_slot (sidebar->window);
			nemo_window_slot_open_location (slot, location,
							    flags, NULL);
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
		NemoWindowSlot *slot;

		gtk_tree_model_get (model, iter,
				    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
				    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
				    -1);

		if (volume != NULL && !sidebar->mounting) {
			sidebar->mounting = TRUE;

			g_assert (sidebar->go_to_after_mount_slot == NULL);

			slot = nemo_window_get_active_slot (sidebar->window);
			sidebar->go_to_after_mount_slot = slot;
			eel_add_weak_pointer (&(sidebar->go_to_after_mount_slot));

			sidebar->go_to_after_mount_flags = flags;

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
open_shortcut_cb (GtkMenuItem		*item,
		  NemoPlacesSidebar	*sidebar)
{
	open_shortcut_from_menu (sidebar, 0);
}

static void
open_shortcut_in_new_window_cb (GtkMenuItem	      *item,
				NemoPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW);
}

static void
open_shortcut_in_new_tab_cb (GtkMenuItem	      *item,
				NemoPlacesSidebar *sidebar)
{
	open_shortcut_from_menu (sidebar, NEMO_WINDOW_OPEN_FLAG_NEW_TAB);
}

/* Add bookmark for the selected item */
static void
add_bookmark (NemoPlacesSidebar *sidebar)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	GFile *location;
	NemoBookmark *bookmark;

	model = gtk_tree_view_get_model (sidebar->tree_view);

	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

		if (uri == NULL) {
			return;
		}

		location = g_file_new_for_uri (uri);
		bookmark = nemo_bookmark_new (location, NULL, NULL);

		if (!nemo_bookmark_list_contains (sidebar->bookmarks, bookmark)) {
			nemo_bookmark_list_append (sidebar->bookmarks, bookmark);
		}

		g_object_unref (location);
		g_object_unref (bookmark);
		g_free (uri);
	}
}

static void
add_shortcut_cb (GtkMenuItem           *item,
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
	GtkCellRenderer *cell;
	GList *renderers;
	PlaceType type;
	
	if (get_selected_iter (sidebar, &iter)) {
		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
				    -1);

		if (type != PLACES_BOOKMARK) {
			return;
		}

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
		column = gtk_tree_view_get_column (GTK_TREE_VIEW (sidebar->tree_view), 0);
		renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
		cell = g_list_nth_data (renderers, 6);
		g_list_free (renderers);
		g_object_set (cell, "editable", TRUE, NULL);
		gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (sidebar->tree_view),
						path, column, cell, TRUE);
		gtk_tree_path_free (path);
	}
}

static void
rename_shortcut_cb (GtkMenuItem           *item,
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

	if (!get_selected_iter (sidebar, &iter)) {
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

	nemo_bookmark_list_delete_item_at (sidebar->bookmarks, index);
}

static void
remove_shortcut_cb (GtkMenuItem           *item,
		    NemoPlacesSidebar *sidebar)
{
	remove_selected_bookmarks (sidebar);
}

static void
mount_shortcut_cb (GtkMenuItem           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GVolume *volume;

	if (!get_selected_iter (sidebar, &iter)) {
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
do_unmount (GMount *mount,
	    NemoPlacesSidebar *sidebar)
{
	if (mount != NULL) {
		nemo_file_operations_unmount_mount_full (NULL, mount, FALSE, TRUE,
							     unmount_done,
							     g_object_ref (sidebar->window));
	}
}

static void
do_unmount_selection (NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;

	if (!get_selected_iter (sidebar, &iter)) {
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
unmount_shortcut_cb (GtkMenuItem           *item,
		     NemoPlacesSidebar *sidebar)
{
	do_unmount_selection (sidebar);
}

static void
drive_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;
	char *primary;
	char *name;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_drive_eject_with_operation_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
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
volume_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;
	char *primary;
	char *name;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_volume_eject_with_operation_finish (G_VOLUME (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_volume_get_name (G_VOLUME (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
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
mount_eject_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NemoWindow *window;
	GError *error;
	char *primary;
	char *name;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_mount_eject_with_operation_finish (G_MOUNT (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_mount_get_name (G_MOUNT (source_object));
			primary = g_strdup_printf (_("Unable to eject %s"), name);
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
do_eject (GMount *mount,
	  GVolume *volume,
	  GDrive *drive,
	  NemoPlacesSidebar *sidebar)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
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
eject_shortcut_cb (GtkMenuItem           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GMount *mount;
	GVolume *volume;
	GDrive *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_MOUNT, &mount,
			    PLACES_SIDEBAR_COLUMN_VOLUME, &volume,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	do_eject (mount, volume, drive, sidebar);
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

	model = GTK_TREE_MODEL (sidebar->store);

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

	if (mount != NULL)
		g_object_unref (mount);
	if (volume != NULL)
		g_object_unref (volume);
	if (drive != NULL)
		g_object_unref (drive);

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
	char *primary;
	char *name;

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to poll %s for media changes"), name);
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
rescan_shortcut_cb (GtkMenuItem           *item,
		    NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		g_drive_poll_for_media (drive, NULL, drive_poll_for_media_cb, NULL);
	}
	g_object_unref (drive);
}

static void
drive_start_cb (GObject      *source_object,
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
start_shortcut_cb (GtkMenuItem           *item,
		   NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
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
	}
	g_object_unref (drive);
}

static void
drive_stop_cb (GObject *source_object,
	       GAsyncResult *res,
	       gpointer user_data)
{
	NemoWindow *window;
	GError *error;
	char *primary;
	char *name;

	window = user_data;
	g_object_unref (window);

	error = NULL;
	if (!g_drive_poll_for_media_finish (G_DRIVE (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			name = g_drive_get_name (G_DRIVE (source_object));
			primary = g_strdup_printf (_("Unable to stop %s"), name);
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
stop_shortcut_cb (GtkMenuItem           *item,
		  NemoPlacesSidebar *sidebar)
{
	GtkTreeIter iter;
	GDrive  *drive;

	if (!get_selected_iter (sidebar, &iter)) {
		return;
	}

	gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store), &iter,
			    PLACES_SIDEBAR_COLUMN_DRIVE, &drive,
			    -1);

	if (drive != NULL) {
		GMountOperation *mount_op;

		mount_op = gtk_mount_operation_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (sidebar))));
		g_drive_stop (drive, G_MOUNT_UNMOUNT_NONE, mount_op, NULL, drive_stop_cb,
			      g_object_ref (sidebar->window));
		g_object_unref (mount_op);
	}
	g_object_unref (drive);
}

static void
empty_trash_cb (GtkMenuItem           *item,
		NemoPlacesSidebar *sidebar)
{
	nemo_file_operations_empty_trash (GTK_WIDGET (sidebar->window));
}

static gboolean
find_prev_or_next_row (NemoPlacesSidebar *sidebar,
		       GtkTreeIter *iter,
		       gboolean go_up)
{
	GtkTreeModel *model = GTK_TREE_MODEL (sidebar->store);
	gboolean res;
	int place_type;

	if (go_up) {
		res = gtk_tree_model_iter_previous (model, iter);
	} else {
		res = gtk_tree_model_iter_next (model, iter);
	}

	if (res) {
		gtk_tree_model_get (model, iter,
				    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type,
				    -1);
		if (place_type == PLACES_HEADING) {
			if (go_up) {
				res = gtk_tree_model_iter_previous (model, iter);
			} else {
				res = gtk_tree_model_iter_next (model, iter);
			}
		}
	}

	return res;
}

static gboolean
find_prev_row (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	return find_prev_or_next_row (sidebar, iter, TRUE);
}

static gboolean
find_next_row (NemoPlacesSidebar *sidebar, GtkTreeIter *iter)
{
	return find_prev_or_next_row (sidebar, iter, FALSE);
}

static void
properties_cb (GtkMenuItem           *item,
	       NemoPlacesSidebar *sidebar)
{
	GtkTreeModel *model;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	GList *list;
	NemoFile *file;
	char *uri;

	model = gtk_tree_view_get_model (sidebar->tree_view);
	gtk_tree_view_get_cursor (sidebar->tree_view, &path, NULL);

	if (path == NULL || !gtk_tree_model_get_iter (model, &iter, path)) {
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_model_get (model, &iter, PLACES_SIDEBAR_COLUMN_URI, &uri, -1);

	if (uri != NULL) {

		file = nemo_file_get_by_uri (uri);
		list = g_list_prepend (NULL, nemo_file_ref (file));

		nemo_properties_window_present (list, GTK_WIDGET (sidebar), NULL);

		nemo_file_list_free (list);
		g_free (uri);
	}

	gtk_tree_path_free (path);
}

static gboolean
nemo_places_sidebar_focus (GtkWidget *widget,
			       GtkDirectionType direction)
{
	NemoPlacesSidebar *sidebar = NEMO_PLACES_SIDEBAR (widget);
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean res;

	res = get_selected_iter (sidebar, &iter);

	if (!res) {
		gtk_tree_model_get_iter_first (GTK_TREE_MODEL (sidebar->store), &iter);
		res = find_next_row (sidebar, &iter);
		if (res) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &iter);
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
  GtkTreePath *path;

  if (!get_selected_iter (sidebar, &selected_iter)) {
	  return FALSE;
  }

  modifiers = gtk_accelerator_get_default_mod_mask ();

  if ((event->keyval == GDK_KEY_Return ||
       event->keyval == GDK_KEY_KP_Enter ||
       event->keyval == GDK_KEY_ISO_Enter ||
       event->keyval == GDK_KEY_space)) {
      NemoWindowOpenFlags flags = 0;

      if ((event->state & modifiers) == GDK_SHIFT_MASK) {
          flags = NEMO_WINDOW_OPEN_FLAG_NEW_TAB;
      } else if ((event->state & modifiers) == GDK_CONTROL_MASK) {
          flags = NEMO_WINDOW_OPEN_FLAG_NEW_WINDOW;
      }

      open_selected_bookmark (sidebar, GTK_TREE_MODEL (sidebar->store),
			      &selected_iter, flags);
      return TRUE;
  }

  if (event->keyval == GDK_KEY_Down &&
      (event->state & modifiers) == GDK_MOD1_MASK) {
      return eject_or_unmount_selection (sidebar);
  }

  if (event->keyval == GDK_KEY_Up) {
      if (find_prev_row (sidebar, &selected_iter)) {
	      path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &selected_iter);
	      gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
	      gtk_tree_path_free (path);
      }
      return TRUE;
  }

  if (event->keyval == GDK_KEY_Down) {
      if (find_next_row (sidebar, &selected_iter)) {
	      path = gtk_tree_model_get_path (GTK_TREE_MODEL (sidebar->store), &selected_iter);
	      gtk_tree_view_set_cursor (sidebar->tree_view, path, NULL, FALSE);
	      gtk_tree_path_free (path);
      }
      return TRUE;
  }

  if ((event->keyval == GDK_KEY_Delete
      || event->keyval == GDK_KEY_KP_Delete)
      && (event->state & modifiers) == 0) {
      remove_selected_bookmarks (sidebar);
      return TRUE;
  }

  if ((event->keyval == GDK_KEY_F2)
      && (event->state & modifiers) == 0) {
      rename_selected_bookmark (sidebar);
      return TRUE;
  }

  return FALSE;
}

/* Constructs the popup menu for the file list if needed */
static void
bookmarks_build_popup_menu (NemoPlacesSidebar *sidebar)
{
	GtkWidget *item;
	gboolean use_browser;
	
	if (sidebar->popup_menu) {
		return;
	}

	use_browser = g_settings_get_boolean (nemo_preferences,
					      NEMO_PREFERENCES_ALWAYS_USE_BROWSER);

	sidebar->popup_menu = gtk_menu_new ();
	gtk_menu_attach_to_widget (GTK_MENU (sidebar->popup_menu),
			           GTK_WIDGET (sidebar),
			           bookmarks_popup_menu_detach_cb);
	
	item = gtk_image_menu_item_new_with_mnemonic (_("_Open"));
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				       gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Tab"));
	sidebar->popup_menu_open_in_new_tab_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_in_new_tab_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	if (use_browser) {
		gtk_widget_show (item);
	}

	item = gtk_menu_item_new_with_mnemonic (_("Open in New _Window"));
	g_signal_connect (item, "activate",
			  G_CALLBACK (open_shortcut_in_new_window_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	if (use_browser) {
		gtk_widget_show (item);
	}

	eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu));

	item = gtk_menu_item_new_with_mnemonic (_("_Add Bookmark"));
	sidebar->popup_menu_add_shortcut_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (add_shortcut_cb), sidebar);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_image_menu_item_new_with_label (_("Remove"));
	sidebar->popup_menu_remove_item = item;
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				 gtk_image_new_from_stock (GTK_STOCK_REMOVE, GTK_ICON_SIZE_MENU));
	g_signal_connect (item, "activate",
		    G_CALLBACK (remove_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);
	
	item = gtk_menu_item_new_with_label (_("Rename..."));
	sidebar->popup_menu_rename_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rename_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);
	
	/* Mount/Unmount/Eject menu items */

	sidebar->popup_menu_separator_item =
		GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

	item = gtk_menu_item_new_with_mnemonic (_("_Mount"));
	sidebar->popup_menu_mount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (mount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Unmount"));
	sidebar->popup_menu_unmount_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (unmount_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Eject"));
	sidebar->popup_menu_eject_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (eject_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Detect Media"));
	sidebar->popup_menu_rescan_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (rescan_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Start"));
	sidebar->popup_menu_start_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (start_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	item = gtk_menu_item_new_with_mnemonic (_("_Stop"));
	sidebar->popup_menu_stop_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (stop_shortcut_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Empty Trash menu item */

	item = gtk_menu_item_new_with_mnemonic (_("Empty _Trash"));
	sidebar->popup_menu_empty_trash_item = item;
	g_signal_connect (item, "activate",
		    G_CALLBACK (empty_trash_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	/* Properties menu item */

	sidebar->popup_menu_properties_separator_item =
		GTK_WIDGET (eel_gtk_menu_append_separator (GTK_MENU (sidebar->popup_menu)));

	item = gtk_menu_item_new_with_mnemonic (_("_Properties"));
	sidebar->popup_menu_properties_item = item;
	g_signal_connect (item, "activate",
			  G_CALLBACK (properties_cb), sidebar);
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (sidebar->popup_menu), item);

	bookmarks_check_popup_sensitivity (sidebar);
}

static void
bookmarks_update_popup_menu (NemoPlacesSidebar *sidebar)
{
	bookmarks_build_popup_menu (sidebar);  
}

static void
bookmarks_popup_menu (NemoPlacesSidebar *sidebar,
		      GdkEventButton        *event)
{
	bookmarks_update_popup_menu (sidebar);
	eel_pop_up_context_menu (GTK_MENU(sidebar->popup_menu),
				 event);
}

/* Callback used for the GtkWidget::popup-menu signal of the shortcuts list */
static gboolean
bookmarks_popup_menu_cb (GtkWidget *widget,
			 NemoPlacesSidebar *sidebar)
{
	bookmarks_popup_menu (sidebar, NULL);
	return TRUE;
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

static void
update_eject_buttons (NemoPlacesSidebar *sidebar,
		      GtkTreePath 	    *path)
{
	GtkTreeIter iter;
	gboolean icon_visible, path_same;

	icon_visible = TRUE;

	if (path == NULL && sidebar->eject_highlight_path == NULL) {
		/* Both are null - highlight up to date */
		return;
	}

	path_same = (path != NULL) &&
		(sidebar->eject_highlight_path != NULL) &&
		(gtk_tree_path_compare (sidebar->eject_highlight_path, path) == 0);

	if (path_same) {
		/* Same path - highlight up to date */
		return;
	}

	if (path) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
					 &iter,
					 path);

		gtk_tree_model_get (GTK_TREE_MODEL (sidebar->store),
				    &iter,
				    PLACES_SIDEBAR_COLUMN_EJECT, &icon_visible,
				    -1);
	}

	if (!icon_visible || path == NULL || !path_same) {
		/* remove highlighting and reset the saved path, as we are leaving
		 * an eject button area.
		 */
		if (sidebar->eject_highlight_path) {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
						 &iter,
						 sidebar->eject_highlight_path);

			gtk_list_store_set (sidebar->store,
					    &iter,
					    PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (sidebar, FALSE),
					    -1);

			gtk_tree_path_free (sidebar->eject_highlight_path);
			sidebar->eject_highlight_path = NULL;
		}

		if (!icon_visible) {
			return;
		}
	}

	if (path != NULL) {
		/* add highlighting to the selected path, as the icon is visible and
		 * we're hovering it.
		 */
		gtk_tree_model_get_iter (GTK_TREE_MODEL (sidebar->store),
					 &iter,
					 path);
		gtk_list_store_set (sidebar->store,
				    &iter,
				    PLACES_SIDEBAR_COLUMN_EJECT_ICON, get_eject_icon (sidebar, TRUE),
				    -1);

		sidebar->eject_highlight_path = gtk_tree_path_copy (path);
	}
}

static gboolean
bookmarks_motion_event_cb (GtkWidget             *widget,
			   GdkEventMotion        *event,
			   NemoPlacesSidebar *sidebar)
{
	GtkTreePath *path;

	path = NULL;

	if (over_eject_button (sidebar, event->x, event->y, &path)) {
		update_eject_buttons (sidebar, path);
		gtk_tree_path_free (path);

		return TRUE;
	}

	update_eject_buttons (sidebar, NULL);

	return FALSE;
}

/* Callback used when a button is pressed on the shortcuts list.  
 * We trap button 3 to bring up a popup menu, and button 2 to
 * open in a new tab.
 */
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

	bookmarks_check_popup_sensitivity (sidebar);
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
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", TRUE,
			      NULL);
	}
}

static void
padding_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", FALSE,
			      "xpad", 0,
			      "ypad", 0,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", TRUE,
			      "xpad", 3,
			      "ypad", 3,
			      NULL);
	}
}

static void
heading_cell_renderer_func (GtkTreeViewColumn *column,
			    GtkCellRenderer *cell,
			    GtkTreeModel *model,
			    GtkTreeIter *iter,
			    gpointer user_data)
{
	PlaceType type;

	gtk_tree_model_get (model, iter,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &type,
			    -1);

	if (type == PLACES_HEADING) {
		g_object_set (cell,
			      "visible", TRUE,
			      NULL);
	} else {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
	}
}

static gint
places_sidebar_sort_func (GtkTreeModel *model,
			  GtkTreeIter *iter_a,
			  GtkTreeIter *iter_b,
			  gpointer user_data)
{
	SectionType section_type_a, section_type_b;
	PlaceType place_type_a, place_type_b;
	gint retval = 0;

	gtk_tree_model_get (model, iter_a,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type_a,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type_a,
			    -1);
	gtk_tree_model_get (model, iter_b,
			    PLACES_SIDEBAR_COLUMN_SECTION_TYPE, &section_type_b,
			    PLACES_SIDEBAR_COLUMN_ROW_TYPE, &place_type_b,
			    -1);

	/* fall back to the default order if we're not in the
	 * XDG part of the computer section.
	 */
	if ((section_type_a == section_type_b) &&
	    (section_type_a == SECTION_COMPUTER) &&
	    (place_type_a == place_type_b) &&
	    (place_type_a == PLACES_XDG_DIR)) {
		gchar *name_a, *name_b;

		gtk_tree_model_get (model, iter_a,
				    PLACES_SIDEBAR_COLUMN_NAME, &name_a,
				    -1);
		gtk_tree_model_get (model, iter_b,
				    PLACES_SIDEBAR_COLUMN_NAME, &name_b,
				    -1);

		retval = g_utf8_collate (name_a, name_b);

		g_free (name_a);
		g_free (name_b);
	}

	return retval;
}

static void
nemo_places_sidebar_init (NemoPlacesSidebar *sidebar)
{
	GtkTreeView       *tree_view;
	GtkTreeViewColumn *col;
	GtkCellRenderer   *cell;
	GtkTreeSelection  *selection;

	sidebar->volume_monitor = g_volume_monitor_get ();
	
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sidebar),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (sidebar), NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sidebar), GTK_SHADOW_IN);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (sidebar)),
					      GTK_JUNCTION_RIGHT | GTK_JUNCTION_LEFT);

  	/* tree view */
	tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_tree_view_set_headers_visible (tree_view, FALSE);

	col = GTK_TREE_VIEW_COLUMN (gtk_tree_view_column_new ());

	/* initial padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	g_object_set (cell,
		      "xpad", 6,
		      NULL);

	/* headings */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_HEADING_TEXT,
					     NULL);
	g_object_set (cell,
		      "weight", PANGO_WEIGHT_BOLD,
		      "weight-set", TRUE,
		      "ypad", 6,
		      "xpad", 0,
		      NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 heading_cell_renderer_func,
						 sidebar, NULL);

	/* icon padding */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 padding_cell_renderer_func,
						 sidebar, NULL);

	/* icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "pixbuf", PLACES_SIDEBAR_COLUMN_ICON,
					     NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
						 icon_cell_renderer_func,
						 sidebar, NULL);

	/* eject text renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_NAME,
					     "visible", PLACES_SIDEBAR_COLUMN_EJECT,
					     NULL);
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	/* eject icon renderer */
	cell = gtk_cell_renderer_pixbuf_new ();
	sidebar->eject_icon_cell_renderer = cell;
	g_object_set (cell,
		      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "stock-size", GTK_ICON_SIZE_MENU,
		      "xpad", EJECT_BUTTON_XPAD,
		      /* align right, because for some reason gtk+ expands
			 this even though we tell it not to. */
		      "xalign", 1.0,
		      NULL);
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_attributes (col, cell,
					     "visible", PLACES_SIDEBAR_COLUMN_EJECT,
					     "pixbuf", PLACES_SIDEBAR_COLUMN_EJECT_ICON,
					     NULL);

	/* normal text renderer */
	cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
	gtk_tree_view_column_set_attributes (col, cell,
					     "text", PLACES_SIDEBAR_COLUMN_NAME,
					     "visible", PLACES_SIDEBAR_COLUMN_NO_EJECT,
					     "editable-set", PLACES_SIDEBAR_COLUMN_BOOKMARK,
					     NULL);
	g_object_set (cell,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "ellipsize-set", TRUE,
		      NULL);

	g_signal_connect (cell, "edited", 
			  G_CALLBACK (bookmarks_edited), sidebar);
	g_signal_connect (cell, "editing-canceled", 
			  G_CALLBACK (bookmarks_editing_canceled), sidebar);

	/* this is required to align the eject buttons to the right */
	gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), NEMO_ICON_SIZE_SMALLER);
	gtk_tree_view_append_column (tree_view, col);

	sidebar->store = nemo_shortcuts_model_new (sidebar);
	gtk_tree_view_set_tooltip_column (tree_view, PLACES_SIDEBAR_COLUMN_TOOLTIP);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sidebar->store),
					      PLACES_SIDEBAR_COLUMN_NAME,
					      GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sidebar->store),
					 PLACES_SIDEBAR_COLUMN_NAME,
					 places_sidebar_sort_func,
					 sidebar, NULL);

	gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (sidebar->store));
	gtk_container_add (GTK_CONTAINER (sidebar), GTK_WIDGET (tree_view));
	gtk_widget_show (GTK_WIDGET (tree_view));

	gtk_widget_show (GTK_WIDGET (sidebar));
	sidebar->tree_view = tree_view;

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
	gtk_drag_dest_set (GTK_WIDGET (tree_view),
			   0,
			   nemo_shortcuts_drop_targets, G_N_ELEMENTS (nemo_shortcuts_drop_targets),
			   GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK);

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
	g_signal_connect (tree_view, "motion-notify-event",
			  G_CALLBACK (bookmarks_motion_event_cb), sidebar);
	g_signal_connect (tree_view, "button-release-event",
			  G_CALLBACK (bookmarks_button_release_event_cb), sidebar);

	eel_gtk_tree_view_set_activate_on_single_click (sidebar->tree_view,
							TRUE);

	g_signal_connect_swapped (nemo_preferences, "changed::" NEMO_PREFERENCES_DESKTOP_IS_HOME_DIR,
				  G_CALLBACK(desktop_setting_changed_callback),
				  sidebar);

	g_signal_connect_swapped (gnome_background_preferences, "changed::" NEMO_PREFERENCES_SHOW_DESKTOP,
				  G_CALLBACK(desktop_setting_changed_callback),
				  sidebar);

	g_signal_connect_object (nemo_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (trash_state_changed_cb),
				 sidebar, 0);
}

static void
nemo_places_sidebar_dispose (GObject *object)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (object);

	sidebar->window = NULL;
	sidebar->tree_view = NULL;

	g_free (sidebar->uri);
	sidebar->uri = NULL;

	free_drag_data (sidebar);

	if (sidebar->eject_highlight_path != NULL) {
		gtk_tree_path_free (sidebar->eject_highlight_path);
		sidebar->eject_highlight_path = NULL;
	}

	if (sidebar->bookmarks_changed_id != 0) {
		g_signal_handler_disconnect (sidebar->bookmarks,
					     sidebar->bookmarks_changed_id);
		sidebar->bookmarks_changed_id = 0;
	}

	g_clear_object (&sidebar->store);
	g_clear_object (&sidebar->bookmarks);

	eel_remove_weak_pointer (&(sidebar->go_to_after_mount_slot));

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      desktop_setting_changed_callback,
					      sidebar);

	g_signal_handlers_disconnect_by_func (nemo_preferences,
					      bookmarks_popup_menu_detach_cb,
					      sidebar);

	g_signal_handlers_disconnect_by_func (gnome_background_preferences,
					      desktop_setting_changed_callback,
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

	G_OBJECT_CLASS (nemo_places_sidebar_parent_class)->dispose (object);
}

static void
nemo_places_sidebar_class_init (NemoPlacesSidebarClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nemo_places_sidebar_dispose;

	GTK_WIDGET_CLASS (class)->style_set = nemo_places_sidebar_style_set;
	GTK_WIDGET_CLASS (class)->focus = nemo_places_sidebar_focus;
}

static void
nemo_places_sidebar_set_parent_window (NemoPlacesSidebar *sidebar,
					   NemoWindow *window)
{
	NemoWindowSlot *slot;

	sidebar->window = window;

	slot = nemo_window_get_active_slot (window);

	sidebar->bookmarks = nemo_bookmark_list_new ();
	sidebar->uri = nemo_window_slot_get_current_uri (slot);

	sidebar->bookmarks_changed_id =
		g_signal_connect_swapped (sidebar->bookmarks, "changed",
					  G_CALLBACK (update_places),
					  sidebar);

	g_signal_connect_object (window, "loading_uri",
				 G_CALLBACK (loading_uri_callback),
				 sidebar, 0);
			 
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
				  G_CALLBACK (bookmarks_popup_menu_detach_cb), sidebar);

	update_places (sidebar);
}

static void
nemo_places_sidebar_style_set (GtkWidget *widget,
				   GtkStyle  *previous_style)
{
	NemoPlacesSidebar *sidebar;

	sidebar = NEMO_PLACES_SIDEBAR (widget);

	update_places (sidebar);
}

GtkWidget *
nemo_places_sidebar_new (NemoWindow *window)
{
	NemoPlacesSidebar *sidebar;
	
	sidebar = g_object_new (nemo_places_sidebar_get_type (), NULL);
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

	if (place_type != PLACES_HEADING && section_type == SECTION_BOOKMARKS)
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

static GtkListStore *
nemo_shortcuts_model_new (NemoPlacesSidebar *sidebar)
{
	NemoShortcutsModel *model;
	GType model_types[PLACES_SIDEBAR_COLUMN_COUNT] = {
		G_TYPE_INT, 
		G_TYPE_STRING,
		G_TYPE_DRIVE,
		G_TYPE_VOLUME,
		G_TYPE_MOUNT,
		G_TYPE_STRING,
		GDK_TYPE_PIXBUF,
		G_TYPE_INT,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_BOOLEAN,
		G_TYPE_STRING,
		GDK_TYPE_PIXBUF,
		G_TYPE_INT,
		G_TYPE_STRING
	};

	model = g_object_new (NEMO_TYPE_SHORTCUTS_MODEL, NULL);
	model->sidebar = sidebar;

	gtk_list_store_set_column_types (GTK_LIST_STORE (model),
					 PLACES_SIDEBAR_COLUMN_COUNT,
					 model_types);

	return GTK_LIST_STORE (model);
}
