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

#define NEMO_TYPE_ACTION nemo_action_get_type()
#define NEMO_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ACTION, NemoAction))
#define NEMO_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ACTION, NemoActionClass))
#define NEMO_IS_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ACTION))
#define NEMO_IS_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ACTION))
#define NEMO_ACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ACTION, NemoActionClass))


#define SELECTION_SINGLE_KEY "s"
#define SELECTION_MULTIPLE_KEY "m"
#define SELECTION_ANY_KEY "any"
#define SELECTION_NONE_KEY "none"
#define SELECTION_NOT_NONE_KEY "notnone"

#define TOKEN_EXEC_URI_LIST "%U"
#define TOKEN_EXEC_FILE_LIST "%F"
#define TOKEN_EXEC_PARENT "%P"
#define TOKEN_EXEC_FILE_NAME "%f"
#define TOKEN_EXEC_PARENT_NAME "%p"
#define TOKEN_EXEC_DEVICE "%D"
#define TOKEN_EXEC_FILE_NO_EXT "%e"

#define TOKEN_LABEL_FILE_NAME "%N" // Leave in for compatibility, same as TOKEN_EXEC_FILE_NAME


#define ACTION_FILE_GROUP "Nemo Action"

#define KEY_ACTIVE "Active"
#define KEY_NAME "Name"
#define KEY_COMMENT "Comment"
#define KEY_EXEC "Exec"
#define KEY_ICON_NAME "Icon-Name"
#define KEY_STOCK_ID "Stock-Id"
#define KEY_SELECTION "Selection"
#define KEY_EXTENSIONS "Extensions"
#define KEY_MIME_TYPES "Mimetypes"
#define KEY_SEPARATOR "Separator"
#define KEY_QUOTE_TYPE "Quote"
#define KEY_DEPENDENCIES "Dependencies"
#define KEY_CONDITIONS "Conditions"
#define KEY_WHITESPACE "EscapeSpaces"
#define KEY_DOUBLE_ESCAPE_QUOTES "DoubleEscapeQuotes"
#define KEY_TERMINAL "Terminal"

typedef struct _NemoAction NemoAction;
typedef struct _NemoActionClass NemoActionClass;

typedef enum {
    SELECTION_SINGLE = G_MAXINT - 10,
    SELECTION_MULTIPLE,
    SELECTION_NOT_NONE,
    SELECTION_ANY,
    SELECTION_NONE
} SelectionType;

typedef enum {
    QUOTE_TYPE_SINGLE = 0,
    QUOTE_TYPE_DOUBLE,
    QUOTE_TYPE_BACKTICK,
    QUOTE_TYPE_NONE
} QuoteType;

typedef enum {
    TOKEN_NONE = 0,
    TOKEN_PATH_LIST,
    TOKEN_URI_LIST,
    TOKEN_FILE_DISPLAY_NAME,
    TOKEN_PARENT_DISPLAY_NAME,
    TOKEN_PARENT_PATH,
    TOKEN_DEVICE,
    TOKEN_FILE_DISPLAY_NAME_NO_EXT
} TokenType;

struct _NemoAction {
    GtkAction parent;
    gchar *key_file_path;
    SelectionType selection_type;
    gchar **extensions;
    gchar **mimetypes;
    gchar *exec;
    gchar *parent_dir;
    gchar **conditions;
    gchar *separator;
    QuoteType quote_type;
    gchar *orig_label;
    gchar *orig_tt;
    gboolean use_parent_dir;
    GList *dbus;
    guint dbus_recalc_timeout_id;
    GList *gsettings;
    guint gsettings_recalc_timeout_id;
    gboolean dbus_satisfied;
    gboolean gsettings_satisfied;
    gboolean escape_underscores;
    gboolean escape_space;
    gboolean show_in_blank_desktop;
    gboolean run_in_terminal;

    gboolean constructing;
};

struct _NemoActionClass {
	GtkActionClass parent_class;
};

GType         nemo_action_get_type             (void);
NemoAction   *nemo_action_new                  (const gchar *name, const gchar *path);
void          nemo_action_activate             (NemoAction *action, GList *selection, NemoFile *parent);

const gchar  *nemo_action_get_orig_label       (NemoAction *action);
const gchar  *nemo_action_get_orig_tt          (NemoAction *action);
gchar        *nemo_action_get_label            (NemoAction *action, GList *selection, NemoFile *parent);
gchar        *nemo_action_get_tt               (NemoAction *action, GList *selection, NemoFile *parent);
gboolean      nemo_action_get_visibility       (NemoAction *action, GList *selection, NemoFile *parent, gboolean for_places);

#endif /* NEMO_ACTION_H */
