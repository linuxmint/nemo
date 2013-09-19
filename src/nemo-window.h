/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */
/* nemo-window.h: Interface of the main window object */

#ifndef NEMO_WINDOW_H
#define NEMO_WINDOW_H

#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <libnemo-private/nemo-bookmark.h>
#include <libnemo-private/nemo-search-directory.h>

#include "nemo-navigation-state.h"
#include "nemo-view.h"
#include "nemo-window-types.h"

#define NEMO_TYPE_WINDOW nemo_window_get_type()
#define NEMO_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_WINDOW, NemoWindow))
#define NEMO_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_WINDOW, NemoWindowClass))
#define NEMO_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_WINDOW))
#define NEMO_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_WINDOW))
#define NEMO_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_WINDOW, NemoWindowClass))

typedef enum {
        NEMO_WINDOW_SHOW_HIDDEN_FILES_ENABLE,
        NEMO_WINDOW_SHOW_HIDDEN_FILES_DISABLE
} NemoWindowShowHiddenFilesMode;

typedef enum {
        NEMO_WINDOW_NOT_SHOWN,
        NEMO_WINDOW_POSITION_SET,
        NEMO_WINDOW_SHOULD_SHOW
} NemoWindowShowState;

typedef enum {
	NEMO_WINDOW_OPEN_SLOT_NONE = 0,
	NEMO_WINDOW_OPEN_SLOT_APPEND = 1
}  NemoWindowOpenSlotFlags;

#define NEMO_WINDOW_SIDEBAR_PLACES "places"
#define NEMO_WINDOW_SIDEBAR_TREE "tree"

typedef struct NemoWindowDetails NemoWindowDetails;

typedef struct {
        GtkWindowClass parent_spot;

	/* Function pointers for overriding, without corresponding signals */

        void   (* sync_title) (NemoWindow *window,
			       NemoWindowSlot *slot);
        NemoIconInfo * (* get_icon) (NemoWindow *window,
                                         NemoWindowSlot *slot);

        void   (* prompt_for_location) (NemoWindow *window, const char *initial);
        void   (* close) (NemoWindow *window);

        /* Signals used only for keybindings */
        gboolean (* go_up)  (NemoWindow *window,
                             gboolean close);
	void     (* reload) (NemoWindow *window);
} NemoWindowClass;

struct NemoWindow {
        GtkWindow parent_object;
        
        NemoWindowDetails *details;
};

GType            nemo_window_get_type             (void);
void             nemo_window_close                (NemoWindow    *window);

void             nemo_window_connect_content_view (NemoWindow    *window,
						       NemoView      *view);
void             nemo_window_disconnect_content_view (NemoWindow    *window,
							  NemoView      *view);

void             nemo_window_go_to                (NemoWindow    *window,
                                                       GFile             *location);
void             nemo_window_go_to_full           (NemoWindow    *window,
                                                       GFile             *location,
                                                       NemoWindowGoToCallback callback,
                                                       gpointer           user_data);
void             nemo_window_new_tab              (NemoWindow    *window);

GtkUIManager *   nemo_window_get_ui_manager       (NemoWindow    *window);
GtkActionGroup * nemo_window_get_main_action_group (NemoWindow   *window);
NemoNavigationState * 
                 nemo_window_get_navigation_state (NemoWindow    *window);

void                 nemo_window_report_load_complete     (NemoWindow *window,
                                                               NemoView *view);

NemoWindowSlot * nemo_window_get_extra_slot       (NemoWindow *window);
NemoWindowShowHiddenFilesMode
                     nemo_window_get_hidden_files_mode (NemoWindow *window);
void                 nemo_window_set_hidden_files_mode (NemoWindow *window,
                                                            NemoWindowShowHiddenFilesMode  mode);
void                 nemo_window_report_load_underway  (NemoWindow *window,
                                                            NemoView *view);
void                 nemo_window_view_visible          (NemoWindow *window,
                                                            NemoView *view);
NemoWindowSlot * nemo_window_get_active_slot       (NemoWindow *window);
void                 nemo_window_push_status           (NemoWindow *window,
                                                            const char *text);

void     nemo_window_hide_sidebar         (NemoWindow *window);
void     nemo_window_show_sidebar         (NemoWindow *window);
void     nemo_window_back_or_forward      (NemoWindow *window,
                                               gboolean        back,
                                               guint           distance,
                                               gboolean        new_tab);
void     nemo_window_split_view_on        (NemoWindow *window);
void     nemo_window_split_view_off       (NemoWindow *window);
gboolean nemo_window_split_view_showing   (NemoWindow *window);

gboolean nemo_window_disable_chrome_mapping (GValue *value,
                                                 GVariant *variant,
                                                 gpointer user_data);

void     nemo_window_set_sidebar_id (NemoWindow *window,
                                    const gchar *id);

const gchar *    nemo_window_get_sidebar_id (NemoWindow *window);

void    nemo_window_set_show_sidebar (NemoWindow *window,
                                      gboolean show);

gboolean  nemo_window_get_show_sidebar (NemoWindow *window);

const gchar *nemo_window_get_ignore_meta_view_id (NemoWindow *window);
void         nemo_window_set_ignore_meta_view_id (NemoWindow *window, const gchar *id);
gint         nemo_window_get_ignore_meta_zoom_level (NemoWindow *window);
void         nemo_window_set_ignore_meta_zoom_level (NemoWindow *window, gint level);

#endif
