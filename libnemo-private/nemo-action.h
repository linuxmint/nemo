/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#ifndef NEMO_ACTION_H
#define NEMO_ACTION_H

#include <gtk/gtk.h>
#include <glib.h>
#include "nemo-file.h"

// GtkAction were deprecated before auto-free functionality was added.
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkAction, g_object_unref)

#define NEMO_TYPE_ACTION nemo_action_get_type()
G_DECLARE_FINAL_TYPE (NemoAction, nemo_action, NEMO, ACTION, GtkAction)

struct _NemoAction {
    GtkAction parent_instance;

    gchar *uuid; // basename of key_file_path
    gchar *key_file_path;
    gchar *parent_dir;
};

struct _NemoActionClass {
    GtkActionClass parent_class;
};

NemoAction   *nemo_action_new                  (const gchar *name, const gchar *path);
void          nemo_action_activate             (NemoAction *action, GList *selection, NemoFile *parent, GtkWindow *window);

const gchar  *nemo_action_get_orig_label       (NemoAction *action);
const gchar  *nemo_action_get_orig_tt          (NemoAction *action);
gchar        *nemo_action_get_label            (NemoAction *action, GList *selection, NemoFile *parent, GtkWindow *window);
gchar        *nemo_action_get_tt               (NemoAction *action, GList *selection, NemoFile *parent, GtkWindow *window);
void          nemo_action_update_display_state (NemoAction *action, GList *selection, NemoFile *parent, gboolean for_places, GtkWindow *window);

// Layout model overrides
void          nemo_action_override_label       (NemoAction *action, const gchar *label);
void          nemo_action_override_icon        (NemoAction *action, const gchar *icon_name);
#endif /* NEMO_ACTION_H */
