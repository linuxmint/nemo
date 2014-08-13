/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           John Sullivan <sullivan@eazel.com>
 *           Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nemo-window-manage-views.h"

#include "nemo-actions.h"
#include "nemo-application.h"
#include "nemo-pathbar.h"
#include "nemo-window-private.h"
#include "nemo-window-slot.h"
#include "nemo-special-location-bar.h"
#include "nemo-trash-bar.h"
#include "nemo-view-factory.h"
#include "nemo-x-content-bar.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnemo-extension/nemo-location-widget-provider.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-file-utilities.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-metadata.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-private/nemo-monitor.h>
#include <libnemo-private/nemo-profile.h>
#include <libnemo-private/nemo-search-directory.h>
#include <libnemo-private/nemo-action-manager.h>

#define DEBUG_FLAG NEMO_DEBUG_WINDOW
#include <libnemo-private/nemo-debug.h>

/* FIXME bugzilla.gnome.org 41243: 
 * We should use inheritance instead of these special cases
 * for the desktop window.
 */
#include "nemo-desktop-window.h"

/* This number controls a maximum character count for a URL that is
 * displayed as part of a dialog. It's fairly arbitrary -- big enough
 * to allow most "normal" URIs to display in full, but small enough to
 * prevent the dialog from getting insanely wide.
 */
#define MAX_URI_IN_DIALOG_LENGTH 60
/*

static void begin_location_change                     (NemoWindowSlot         *slot,
						       GFile                      *location,
						       GFile                      *previous_location,
						       GList                      *new_selection,
						       NemoLocationChangeType  type,
						       guint                       distance,
						       const char                 *scroll_pos,
						       NemoWindowGoToCallback  callback,
						       gpointer                    user_data);
static void free_location_change                      (NemoWindowSlot         *slot);
static void end_location_change                       (NemoWindowSlot         *slot);
static void cancel_location_change                    (NemoWindowSlot         *slot);
static void got_file_info_for_view_selection_callback (NemoFile               *file,
						       gpointer                    callback_data);
static gboolean create_content_view                   (NemoWindowSlot         *slot,
						       const char                 *view_id,
						       GError                    **error);
static void display_view_selection_failure            (NemoWindow             *window,
						       NemoFile               *file,
						       GFile                      *location,
						       GError                     *error);
static void load_new_location                         (NemoWindowSlot         *slot,
						       GFile                      *location,
						       GList                      *selection,
						       gboolean                    tell_current_content_view,
						       gboolean                    tell_new_content_view);
static void location_has_really_changed               (NemoWindowSlot         *slot);
static void update_for_new_location                   (NemoWindowSlot         *slot);

extern void
set_displayed_file (NemoWindowSlot *slot, NemoFile *file);


static void
check_bookmark_location_matches (NemoBookmark *bookmark, GFile *location);



extern void
check_last_bookmark_location_matches_slot (NemoWindowSlot *slot);

extern void
handle_go_back (NemoWindowSlot *slot,
		GFile *location);

extern void
handle_go_forward (NemoWindowSlot *slot,
		   GFile *location);


extern void
handle_go_elsewhere (NemoWindowSlot *slot,
		     GFile *location);


extern void
viewed_file_changed_callback (NemoFile *file,
                              NemoWindowSlot *slot);


static void
update_history (NemoWindowSlot *slot,
                NemoLocationChangeType type,
                GFile *new_location):

static void
new_window_show_callback (GtkWidget *widget,
			  gpointer user_data);

*/


/*
extern gboolean
nemo_window_slot_content_view_matches_iid (NemoWindowSlot *slot, 
					       const char *iid);

extern gboolean
report_callback (NemoWindowSlot *slot,
		 GError *error);


extern void
begin_location_change (NemoWindowSlot *slot,
                       GFile *location,
                       GFile *previous_location,
		       GList *new_selection,
                       NemoLocationChangeType type,
                       guint distance,
                       const char *scroll_pos,
		       NemoWindowGoToCallback callback,
		       gpointer user_data);


typedef struct {
	GCancellable *cancellable;
	NemoWindowSlot *slot;
} MountNotMountedData;

extern void
mount_not_mounted_callback (GObject *source_object,
			    GAsyncResult *res,
			    gpointer user_data);

extern void
got_file_info_for_view_selection_callback (NemoFile *file,
					   gpointer callback_data);


extern gboolean
create_content_view (NemoWindowSlot *slot,
		     const char *view_id,
		     GError **error_out);

extern void
load_new_location (NemoWindowSlot *slot,
		   GFile *location,
		   GList *selection,
		   gboolean tell_current_content_view,
		   gboolean tell_new_content_view);
*/


/*
extern void
nemo_window_report_location_change (NemoWindow *window);


extern void
location_has_really_changed (NemoWindowSlot *slot);


extern void
slot_add_extension_extra_widgets (NemoWindowSlot *slot);

typedef struct {
	NemoWindowSlot *slot;
	GCancellable *cancellable;
	GMount *mount;
} FindMountData;

extern void
found_content_type_cb (const char **x_content_types,
		       gpointer user_data);


extern void
found_mount_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data);

extern void
update_for_new_location (NemoWindowSlot *slot);
*/


/*
extern void
end_location_change (NemoWindowSlot *slot);

extern void
free_location_change (NemoWindowSlot *slot);

extern void
cancel_location_change (NemoWindowSlot *slot);

extern void
display_view_selection_failure (NemoWindow *window, NemoFile *file,
				GFile *location, GError *error);


void
nemo_window_slot_stop_loading (NemoWindowSlot *slot);


void
nemo_window_slot_set_content_view (NemoWindowSlot *slot,
				       const char *id);
*/


