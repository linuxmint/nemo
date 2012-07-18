/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nemo-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef NEMO_VIEW_H
#define NEMO_VIEW_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-icon-container.h>
#include <libnemo-private/nemo-link.h>

typedef struct NemoView NemoView;
typedef struct NemoViewClass NemoViewClass;

#include "nemo-window.h"
#include "nemo-window-slot.h"

#define NEMO_TYPE_VIEW nemo_view_get_type()
#define NEMO_VIEW(obj)\
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_VIEW, NemoView))
#define NEMO_VIEW_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_VIEW, NemoViewClass))
#define NEMO_IS_VIEW(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_VIEW))
#define NEMO_IS_VIEW_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_VIEW))
#define NEMO_VIEW_GET_CLASS(obj)\
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_VIEW, NemoViewClass))

typedef struct NemoViewDetails NemoViewDetails;

struct NemoView {
	GtkScrolledWindow parent;

	NemoViewDetails *details;
};

struct NemoViewClass {
	GtkScrolledWindowClass parent_class;

	/* The 'clear' signal is emitted to empty the view of its contents.
	 * It must be replaced by each subclass.
	 */
	void 	(* clear) 		 (NemoView *view);
	
	/* The 'begin_file_changes' signal is emitted before a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary preparation for a set of new files. The default
	 * implementation does nothing.
	 */
	void 	(* begin_file_changes) (NemoView *view);
	
	/* The 'add_file' signal is emitted to add one file to the view.
	 * It must be replaced by each subclass.
	 */
	void    (* add_file) 		 (NemoView *view, 
					  NemoFile *file,
					  NemoDirectory *directory);
	void    (* remove_file)		 (NemoView *view, 
					  NemoFile *file,
					  NemoDirectory *directory);

	/* The 'file_changed' signal is emitted to signal a change in a file,
	 * including the file being removed.
	 * It must be replaced by each subclass.
	 */
	void 	(* file_changed)         (NemoView *view, 
					  NemoFile *file,
					  NemoDirectory *directory);

	/* The 'end_file_changes' signal is emitted after a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary cleanup (typically, cleanup for code in begin_file_changes).
	 * The default implementation does nothing.
	 */
	void 	(* end_file_changes)    (NemoView *view);
	
	/* The 'begin_loading' signal is emitted before any of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary preparation to start dealing with a
	 * new directory. The default implementation does nothing.
	 */
	void 	(* begin_loading) 	 (NemoView *view);

	/* The 'end_loading' signal is emitted after all of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary clean-up. The default implementation 
	 * does nothing.
	 *
	 * If all_files_seen is true, the handler may assume that
	 * no load error ocurred, and all files of the underlying
	 * directory were loaded.
	 *
	 * Otherwise, end_loading was emitted due to cancellation,
	 * which usually means that not all files are available.
	 */
	void 	(* end_loading) 	 (NemoView *view,
					  gboolean all_files_seen);

	/* The 'load_error' signal is emitted when the directory model
	 * reports an error in the process of monitoring the directory's
	 * contents.  The load error indicates that the process of 
	 * loading the contents has ended, but the directory is still
	 * being monitored. The default implementation handles common
	 * load failures like ACCESS_DENIED.
	 */
	void    (* load_error)           (NemoView *view,
					  GError *error);

	/* Function pointers that don't have corresponding signals */

        /* reset_to_defaults is a function pointer that subclasses must 
         * override to set sort order, zoom level, etc to match default
         * values. 
         */
        void     (* reset_to_defaults)	         (NemoView *view);

	/* get_backing uri is a function pointer for subclasses to
	 * override. Subclasses may replace it with a function that
	 * returns the URI for the location where to create new folders,
	 * files, links and paste the clipboard to.
	 */

	char *	(* get_backing_uri)		(NemoView *view);

	/* get_selection is not a signal; it is just a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NemoFile pointers.
	 */
	GList *	(* get_selection) 	 	(NemoView *view);
	
	/* get_selection_for_file_transfer  is a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NemoFile pointers. The difference from get_selection is
	 * that any files in the selection that also has a parent folder
	 * in the selection is not included.
	 */
	GList *	(* get_selection_for_file_transfer)(NemoView *view);
	
        /* select_all is a function pointer that subclasses must override to
         * select all of the items in the view */
        void     (* select_all)	         	(NemoView *view);

        /* set_selection is a function pointer that subclasses must
         * override to select the specified items (and unselect all
         * others). The argument is a list of NemoFiles. */

        void     (* set_selection)	 	(NemoView *view, 
        					 GList *selection);
        					 
        /* invert_selection is a function pointer that subclasses must
         * override to invert selection. */

        void     (* invert_selection)	 	(NemoView *view);        					 

	/* Return an array of locations of selected icons in their view. */
	GArray * (* get_selected_icon_locations) (NemoView *view);

	guint    (* get_item_count)             (NemoView *view);

        /* bump_zoom_level is a function pointer that subclasses must override
         * to change the zoom level of an object. */
        void    (* bump_zoom_level)      	(NemoView *view,
					  	 int zoom_increment);

        /* zoom_to_level is a function pointer that subclasses must override
         * to set the zoom level of an object to the specified level. */
        void    (* zoom_to_level) 		(NemoView *view, 
        				         NemoZoomLevel level);

        NemoZoomLevel (* get_zoom_level)    (NemoView *view);

	/* restore_default_zoom_level is a function pointer that subclasses must override
         * to restore the zoom level of an object to a default setting. */
        void    (* restore_default_zoom_level) (NemoView *view);

        /* can_zoom_in is a function pointer that subclasses must override to
         * return whether the view is at maximum size (furthest-in zoom level) */
        gboolean (* can_zoom_in)	 	(NemoView *view);

        /* can_zoom_out is a function pointer that subclasses must override to
         * return whether the view is at minimum size (furthest-out zoom level) */
        gboolean (* can_zoom_out)	 	(NemoView *view);
        
        /* reveal_selection is a function pointer that subclasses may
         * override to make sure the selected items are sufficiently
         * apparent to the user (e.g., scrolled into view). By default,
         * this does nothing.
         */
        void     (* reveal_selection)	 	(NemoView *view);

        /* merge_menus is a function pointer that subclasses can override to
         * add their own menu items to the window's menu bar.
         * If overridden, subclasses must call parent class's function.
         */
        void    (* merge_menus)         	(NemoView *view);
        void    (* unmerge_menus)         	(NemoView *view);

        /* update_menus is a function pointer that subclasses can override to
         * update the sensitivity or wording of menu items in the menu bar.
         * It is called (at least) whenever the selection changes. If overridden, 
         * subclasses must call parent class's function.
         */
        void    (* update_menus)         	(NemoView *view);

	/* sort_files is a function pointer that subclasses can override
	 * to provide a sorting order to determine which files should be
	 * presented when only a partial list is provided.
	 */
	int     (* compare_files)              (NemoView *view,
						NemoFile    *a,
						NemoFile    *b);

	/* using_manual_layout is a function pointer that subclasses may
	 * override to control whether or not items can be freely positioned
	 * on the user-visible area.
	 * Note that this value is not guaranteed to be constant within the
	 * view's lifecycle. */
	gboolean (* using_manual_layout)     (NemoView *view);

	/* is_read_only is a function pointer that subclasses may
	 * override to control whether or not the user is allowed to
	 * change the contents of the currently viewed directory. The
	 * default implementation checks the permissions of the
	 * directory.
	 */
	gboolean (* is_read_only)	        (NemoView *view);

	/* is_empty is a function pointer that subclasses must
	 * override to report whether the view contains any items.
	 */
	gboolean (* is_empty)                   (NemoView *view);

	gboolean (* can_rename_file)            (NemoView *view,
						 NemoFile *file);
	/* select_all specifies whether the whole filename should be selected
	 * or only its basename (i.e. everything except the extension)
	 * */
	void	 (* start_renaming_file)        (NemoView *view,
					  	 NemoFile *file,
						 gboolean select_all);

	/* convert *point from widget's coordinate system to a coordinate
	 * system used for specifying file operation positions, which is view-specific.
	 *
	 * This is used by the the icon view, which converts the screen position to a zoom
	 * level-independent coordinate system.
	 */
	void (* widget_to_file_operation_position) (NemoView *view,
						    GdkPoint     *position);

	/* Preference change callbacks, overriden by icon and list views. 
	 * Icon and list views respond by synchronizing to the new preference
	 * values and forcing an update if appropriate.
	 */
	void	(* click_policy_changed)	   (NemoView *view);
	void	(* sort_directories_first_changed) (NemoView *view);

	/* Get the id string for this view. Its a constant string, not memory managed */
	const char *   (* get_view_id)            (NemoView          *view);

	/* Return the uri of the first visible file */	
	char *         (* get_first_visible_file) (NemoView          *view);
	/* Scroll the view so that the file specified by the uri is at the top
	   of the view */
	void           (* scroll_to_file)	  (NemoView          *view,
						   const char            *uri);

        /* Signals used only for keybindings */
        gboolean (* trash)                         (NemoView *view);
        gboolean (* delete)                        (NemoView *view);
};

/* GObject support */
GType               nemo_view_get_type                         (void);

/* Functions callable from the user interface and elsewhere. */
NemoWindow     *nemo_view_get_nemo_window              (NemoView  *view);
NemoWindowSlot *nemo_view_get_nemo_window_slot     (NemoView  *view);
char *              nemo_view_get_uri                          (NemoView  *view);

void                nemo_view_display_selection_info           (NemoView  *view);

GdkAtom	            nemo_view_get_copied_files_atom            (NemoView  *view);
gboolean            nemo_view_get_active                       (NemoView  *view);

/* Wrappers for signal emitters. These are normally called 
 * only by NemoView itself. They have corresponding signals
 * that observers might want to connect with.
 */
gboolean            nemo_view_get_loading                      (NemoView  *view);

/* Hooks for subclasses to call. These are normally called only by 
 * NemoView and its subclasses 
 */
void                nemo_view_activate_files                   (NemoView        *view,
								    GList                  *files,
								    NemoWindowOpenFlags flags,
								    gboolean                confirm_multiple);
void                nemo_view_preview_files                    (NemoView        *view,
								    GList               *files,
								    GArray              *locations);
void                nemo_view_start_batching_selection_changes (NemoView  *view);
void                nemo_view_stop_batching_selection_changes  (NemoView  *view);
void                nemo_view_notify_selection_changed         (NemoView  *view);
GtkUIManager *      nemo_view_get_ui_manager                   (NemoView  *view);
NemoDirectory  *nemo_view_get_model                        (NemoView  *view);
NemoFile       *nemo_view_get_directory_as_file            (NemoView  *view);
void                nemo_view_pop_up_background_context_menu   (NemoView  *view,
								    GdkEventButton   *event);
void                nemo_view_pop_up_selection_context_menu    (NemoView  *view,
								    GdkEventButton   *event); 
gboolean            nemo_view_should_show_file                 (NemoView  *view,
								    NemoFile     *file);
gboolean	    nemo_view_should_sort_directories_first    (NemoView  *view);
void                nemo_view_ignore_hidden_file_preferences   (NemoView  *view);
void                nemo_view_set_show_foreign                 (NemoView  *view,
								    gboolean          show_foreign);
gboolean            nemo_view_handle_scroll_event              (NemoView  *view,
								    GdkEventScroll   *event);

void                nemo_view_freeze_updates                   (NemoView  *view);
void                nemo_view_unfreeze_updates                 (NemoView  *view);
gboolean            nemo_view_get_is_renaming                  (NemoView  *view);
void                nemo_view_set_is_renaming                  (NemoView  *view,
								    gboolean       renaming);
void                nemo_view_add_subdirectory                (NemoView  *view,
								   NemoDirectory*directory);
void                nemo_view_remove_subdirectory             (NemoView  *view,
								   NemoDirectory*directory);

gboolean            nemo_view_is_editable                     (NemoView *view);

/* NemoView methods */
const char *      nemo_view_get_view_id                (NemoView      *view);

/* file operations */
char *            nemo_view_get_backing_uri            (NemoView      *view);
void              nemo_view_move_copy_items            (NemoView      *view,
							    const GList       *item_uris,
							    GArray            *relative_item_points,
							    const char        *target_uri,
							    int                copy_action,
							    int                x,
							    int                y);
void              nemo_view_new_file_with_initial_contents (NemoView *view,
								const char *parent_uri,
								const char *filename,
								const char *initial_contents,
								int length,
								GdkPoint *pos);

/* selection handling */
int               nemo_view_get_selection_count        (NemoView      *view);
GList *           nemo_view_get_selection              (NemoView      *view);
void              nemo_view_set_selection              (NemoView      *view,
							    GList             *selection);


void              nemo_view_load_location              (NemoView      *view,
							    GFile             *location);
void              nemo_view_stop_loading               (NemoView      *view);

char **           nemo_view_get_emblem_names_to_exclude (NemoView     *view);
char *            nemo_view_get_first_visible_file     (NemoView      *view);
void              nemo_view_scroll_to_file             (NemoView      *view,
							    const char        *uri);
char *            nemo_view_get_title                  (NemoView      *view);
gboolean          nemo_view_supports_zooming           (NemoView      *view);
void              nemo_view_bump_zoom_level            (NemoView      *view,
							    int                zoom_increment);
void              nemo_view_zoom_to_level              (NemoView      *view,
							    NemoZoomLevel  level);
void              nemo_view_restore_default_zoom_level (NemoView      *view);
gboolean          nemo_view_can_zoom_in                (NemoView      *view);
gboolean          nemo_view_can_zoom_out               (NemoView      *view);
NemoZoomLevel nemo_view_get_zoom_level             (NemoView      *view);
void              nemo_view_pop_up_location_context_menu (NemoView    *view,
							      GdkEventButton  *event,
							      const char      *location);
void              nemo_view_grab_focus                 (NemoView      *view);
void              nemo_view_update_menus               (NemoView      *view);

#endif /* NEMO_VIEW_H */
