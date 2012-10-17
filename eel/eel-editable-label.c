/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>
#include <math.h>
#include <string.h>

#include "eel-editable-label.h"
#include "eel-accessibility.h"
#include <libgail-util/gailmisc.h>

#include <glib/gi18n-lib.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

enum {
  MOVE_CURSOR,
  POPULATE_POPUP,
  DELETE_FROM_CURSOR,
  CUT_CLIPBOARD,
  COPY_CLIPBOARD,
  PASTE_CLIPBOARD,
  TOGGLE_OVERWRITE,
  LAST_SIGNAL
};

enum {
  PROP_0,
  PROP_TEXT,
  PROP_JUSTIFY,
  PROP_WRAP,
  PROP_CURSOR_POSITION,
  PROP_SELECTION_BOUND
};

static guint signals[LAST_SIGNAL] = { 0 };

static void     eel_editable_label_editable_init           (GtkEditableInterface  *iface);
static void     eel_editable_label_class_init              (EelEditableLabelClass *klass);
static void     eel_editable_label_init                    (EelEditableLabel      *label);
static void     eel_editable_label_set_property            (GObject               *object,
							    guint                  prop_id,
							    const GValue          *value,
							    GParamSpec            *pspec);
static void     eel_editable_label_get_property            (GObject               *object,
							    guint                  prop_id,
							    GValue                *value,
							    GParamSpec            *pspec);
static void     eel_editable_label_finalize                (GObject               *object);
static void     eel_editable_label_get_preferred_width     (GtkWidget             *widget,
                                                            gint                  *minimum,
                                                            gint                  *natural);
static void     eel_editable_label_get_preferred_height    (GtkWidget             *widget,
                                                            gint                  *minimum,
                                                            gint                  *natural);
static void     eel_editable_label_size_allocate           (GtkWidget             *widget,
							    GtkAllocation         *allocation);
static void     eel_editable_label_state_changed           (GtkWidget             *widget,
							    GtkStateType           state);
static void     eel_editable_label_style_updated           (GtkWidget             *widget);
static void     eel_editable_label_direction_changed       (GtkWidget             *widget,
							    GtkTextDirection       previous_dir);
static gint     eel_editable_label_draw                    (GtkWidget             *widget,
							    cairo_t               *cr);
static void     eel_editable_label_realize                 (GtkWidget             *widget);
static void     eel_editable_label_unrealize               (GtkWidget             *widget);
static void     eel_editable_label_map                     (GtkWidget             *widget);
static void     eel_editable_label_unmap                   (GtkWidget             *widget);
static gint     eel_editable_label_button_press            (GtkWidget             *widget,
							    GdkEventButton        *event);
static gint     eel_editable_label_button_release          (GtkWidget             *widget,
							    GdkEventButton        *event);
static gint     eel_editable_label_motion                  (GtkWidget             *widget,
							    GdkEventMotion        *event);
static gint     eel_editable_label_key_press               (GtkWidget             *widget,
							    GdkEventKey           *event);
static gint     eel_editable_label_key_release             (GtkWidget             *widget,
							    GdkEventKey           *event);
static gint     eel_editable_label_focus_in                (GtkWidget             *widget,
							    GdkEventFocus         *event);
static gint     eel_editable_label_focus_out               (GtkWidget             *widget,
							    GdkEventFocus         *event);
static AtkObject *eel_editable_label_get_accessible        (GtkWidget             *widget);
static void     eel_editable_label_commit_cb               (GtkIMContext          *context,
							    const gchar           *str,
							    EelEditableLabel      *label);
static void     eel_editable_label_preedit_changed_cb      (GtkIMContext          *context,
							    EelEditableLabel      *label);
static gboolean eel_editable_label_retrieve_surrounding_cb (GtkIMContext          *context,
							    EelEditableLabel      *label);
static gboolean eel_editable_label_delete_surrounding_cb   (GtkIMContext          *slave,
							    gint                   offset,
							    gint                   n_chars,
							    EelEditableLabel      *label);
static void     eel_editable_label_clear_layout            (EelEditableLabel      *label);
static void     eel_editable_label_recompute               (EelEditableLabel      *label);
static void     eel_editable_label_ensure_layout           (EelEditableLabel      *label,
							    gboolean               include_preedit);
static void     eel_editable_label_select_region_index     (EelEditableLabel      *label,
							    gint                   anchor_index,
							    gint                   end_index);
static gboolean eel_editable_label_focus                   (GtkWidget             *widget,
							    GtkDirectionType       direction);
static void     eel_editable_label_move_cursor             (EelEditableLabel      *label,
							    GtkMovementStep        step,
							    gint                   count,
							    gboolean               extend_selection);
static void     eel_editable_label_delete_from_cursor      (EelEditableLabel      *label,
							    GtkDeleteType          type,
							    gint                   count);
static void     eel_editable_label_copy_clipboard          (EelEditableLabel      *label);
static void     eel_editable_label_cut_clipboard           (EelEditableLabel      *label);
static void     eel_editable_label_paste                   (EelEditableLabel      *label,
							    GdkAtom                selection);
static void     eel_editable_label_paste_clipboard         (EelEditableLabel      *label);
static void     eel_editable_label_select_all              (EelEditableLabel      *label);
static void     eel_editable_label_do_popup                (EelEditableLabel      *label,
							    GdkEventButton        *event);
static void     eel_editable_label_toggle_overwrite        (EelEditableLabel      *label);
static gint     eel_editable_label_move_forward_word       (EelEditableLabel      *label,
							    gint                   start);
static gint     eel_editable_label_move_backward_word      (EelEditableLabel      *label,
							    gint                   start);
static void     eel_editable_label_reset_im_context        (EelEditableLabel      *label);
static void     eel_editable_label_check_cursor_blink      (EelEditableLabel      *label);
static void     eel_editable_label_pend_cursor_blink       (EelEditableLabel      *label);

/* Editable implementation: */
static void     editable_insert_text_emit     (GtkEditable *editable,
					       const gchar *new_text,
					       gint         new_text_length,
					       gint        *position);
static void     editable_delete_text_emit     (GtkEditable *editable,
					       gint         start_pos,
					       gint         end_pos);
static void     editable_insert_text          (GtkEditable *editable,
					       const gchar *new_text,
					       gint         new_text_length,
					       gint        *position);
static void     editable_delete_text          (GtkEditable *editable,
					       gint         start_pos,
					       gint         end_pos);
static gchar *  editable_get_chars            (GtkEditable *editable,
					       gint         start_pos,
					       gint         end_pos);
static void     editable_set_selection_bounds (GtkEditable *editable,
					       gint         start,
					       gint         end);
static gboolean editable_get_selection_bounds (GtkEditable *editable,
					       gint        *start,
					       gint        *end);
static void     editable_real_set_position    (GtkEditable *editable,
					       gint         position);
static gint     editable_get_position         (GtkEditable *editable);

G_DEFINE_TYPE_WITH_CODE (EelEditableLabel, eel_editable_label, GTK_TYPE_MISC,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_EDITABLE, eel_editable_label_editable_init));

static void
add_move_binding (GtkBindingSet  *binding_set,
		  guint           keyval,
		  guint           modmask,
		  GtkMovementStep step,
		  gint            count)
{
  g_assert ((modmask & GDK_SHIFT_MASK) == 0);
  
  gtk_binding_entry_add_signal (binding_set, keyval, modmask,
				"move_cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, FALSE);

  /* Selection-extending version */
  gtk_binding_entry_add_signal (binding_set, keyval, modmask | GDK_SHIFT_MASK,
				"move_cursor", 3,
				G_TYPE_ENUM, step,
				G_TYPE_INT, count,
                                G_TYPE_BOOLEAN, TRUE);
}

static void
eel_editable_label_class_init (EelEditableLabelClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
  GtkBindingSet *binding_set;

  gobject_class->set_property = eel_editable_label_set_property;
  gobject_class->get_property = eel_editable_label_get_property;
  gobject_class->finalize = eel_editable_label_finalize;

  widget_class->get_preferred_width = eel_editable_label_get_preferred_width;
  widget_class->get_preferred_height = eel_editable_label_get_preferred_height;
  widget_class->size_allocate = eel_editable_label_size_allocate;
  widget_class->state_changed = eel_editable_label_state_changed;
  widget_class->style_updated = eel_editable_label_style_updated;
  widget_class->direction_changed = eel_editable_label_direction_changed;
  widget_class->draw = eel_editable_label_draw;
  widget_class->realize = eel_editable_label_realize;
  widget_class->unrealize = eel_editable_label_unrealize;
  widget_class->map = eel_editable_label_map;
  widget_class->unmap = eel_editable_label_unmap;
  widget_class->button_press_event = eel_editable_label_button_press;
  widget_class->button_release_event = eel_editable_label_button_release;
  widget_class->motion_notify_event = eel_editable_label_motion;
  widget_class->focus = eel_editable_label_focus;
  widget_class->key_press_event = eel_editable_label_key_press;
  widget_class->key_release_event = eel_editable_label_key_release;
  widget_class->focus_in_event = eel_editable_label_focus_in;
  widget_class->focus_out_event = eel_editable_label_focus_out;
  widget_class->get_accessible = eel_editable_label_get_accessible;

  class->move_cursor = eel_editable_label_move_cursor;
  class->delete_from_cursor = eel_editable_label_delete_from_cursor;
  class->copy_clipboard = eel_editable_label_copy_clipboard;
  class->cut_clipboard = eel_editable_label_cut_clipboard;
  class->paste_clipboard = eel_editable_label_paste_clipboard;
  class->toggle_overwrite = eel_editable_label_toggle_overwrite;
  
  signals[MOVE_CURSOR] = 
    g_signal_new ("move_cursor",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EelEditableLabelClass, move_cursor),
		  NULL, NULL,
		  g_cclosure_marshal_generic,
		  G_TYPE_NONE, 3, GTK_TYPE_MOVEMENT_STEP, G_TYPE_INT, G_TYPE_BOOLEAN);
  
  signals[COPY_CLIPBOARD] =
    g_signal_new ("copy_clipboard",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET  (EelEditableLabelClass, copy_clipboard),
		  NULL, NULL, 
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
  signals[POPULATE_POPUP] =
    g_signal_new ("populate_popup",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EelEditableLabelClass, populate_popup),
		  NULL, NULL, 
		  g_cclosure_marshal_VOID__OBJECT,
		  G_TYPE_NONE, 1, GTK_TYPE_MENU);

  signals[DELETE_FROM_CURSOR] = 
    g_signal_new ("delete_from_cursor",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EelEditableLabelClass, delete_from_cursor),
		  NULL, NULL, 
		  g_cclosure_marshal_generic,
		  G_TYPE_NONE, 2, GTK_TYPE_DELETE_TYPE, G_TYPE_INT);
  
  signals[CUT_CLIPBOARD] =
    g_signal_new ("cut_clipboard",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EelEditableLabelClass, cut_clipboard),
		  NULL, NULL, 
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  signals[PASTE_CLIPBOARD] =
    g_signal_new ("paste_clipboard",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EelEditableLabelClass, paste_clipboard),
		  NULL, NULL, 
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  signals[TOGGLE_OVERWRITE] =
    g_signal_new ("toggle_overwrite",
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (EelEditableLabelClass, toggle_overwrite),
		  NULL, NULL, 
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        _("Text"),
                                                        _("The text of the label."),
                                                        NULL,
                                                        G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
				   PROP_JUSTIFY,
                                   g_param_spec_enum ("justify",
                                                      _("Justification"),
                                                      _("The alignment of the lines in the text of the label relative to each other. This does NOT affect the alignment of the label within its allocation. See GtkMisc::xalign for that."),
						      GTK_TYPE_JUSTIFICATION,
						      GTK_JUSTIFY_LEFT,
                                                      G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_WRAP,
                                   g_param_spec_boolean ("wrap",
                                                        _("Line wrap"),
                                                        _("If set, wrap lines if the text becomes too wide."),
                                                        FALSE,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class,
                                   PROP_CURSOR_POSITION,
                                   g_param_spec_int ("cursor_position",
                                                     _("Cursor Position"),
                                                     _("The current position of the insertion cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  g_object_class_install_property (gobject_class,
                                   PROP_SELECTION_BOUND,
                                   g_param_spec_int ("selection_bound",
                                                     _("Selection Bound"),
                                                     _("The position of the opposite end of the selection from the cursor in chars."),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE));
  
  /*
   * Key bindings
   */

  binding_set = gtk_binding_set_by_class (class);

  /* Moving the insertion point */
  add_move_binding (binding_set, GDK_KEY_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Right, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_KP_Left, 0,
		    GTK_MOVEMENT_VISUAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_KEY_f, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, 1);
  
  add_move_binding (binding_set, GDK_KEY_b, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_LOGICAL_POSITIONS, -1);
  
  add_move_binding (binding_set, GDK_KEY_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Right, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Left, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_WORDS, -1);
  
  add_move_binding (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_e, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_PARAGRAPH_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_f, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, 1);

  add_move_binding (binding_set, GDK_KEY_b, GDK_MOD1_MASK,
		    GTK_MOVEMENT_WORDS, -1);

  add_move_binding (binding_set, GDK_KEY_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Home, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_End, 0,
		    GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
  
  add_move_binding (binding_set, GDK_KEY_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Home, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, -1);

  add_move_binding (binding_set, GDK_KEY_KP_End, GDK_CONTROL_MASK,
		    GTK_MOVEMENT_BUFFER_ENDS, 1);

  add_move_binding (binding_set, GDK_KEY_Up, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);

  add_move_binding (binding_set, GDK_KEY_KP_Up, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, -1);
  
  add_move_binding (binding_set, GDK_KEY_Down, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);

  add_move_binding (binding_set, GDK_KEY_KP_Down, 0,
                    GTK_MOVEMENT_DISPLAY_LINES, 1);
  
  /* Select all
   */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                "move_cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, -1,
				G_TYPE_BOOLEAN, FALSE);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_a, GDK_CONTROL_MASK,
                                "move_cursor", 3,
                                GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_BUFFER_ENDS,
                                G_TYPE_INT, 1,
				G_TYPE_BOOLEAN, TRUE);
  
  /* Deleting text */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, 0,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, 0,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, 0,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_CHARS,
				G_TYPE_INT, -1);

  /* Make this do the same as Backspace, to help with mis-typing */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_SHIFT_MASK,
                                "delete_from_cursor", 2,
                                G_TYPE_ENUM, GTK_DELETE_CHARS,
                                G_TYPE_INT, -1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Delete, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, 1);
  
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_BackSpace, GDK_CONTROL_MASK,
				"delete_from_cursor", 2,
				G_TYPE_ENUM, GTK_DELETE_WORD_ENDS,
				G_TYPE_INT, -1);

  /* Cut/copy/paste */

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_x, GDK_CONTROL_MASK,
				"cut_clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_c, GDK_CONTROL_MASK,
				"copy_clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_v, GDK_CONTROL_MASK,
				"paste_clipboard", 0);

  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Delete, GDK_SHIFT_MASK,
				"cut_clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, GDK_CONTROL_MASK,
				"copy_clipboard", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, GDK_SHIFT_MASK,
				"paste_clipboard", 0);

  /* Overwrite */
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_Insert, 0,
				"toggle_overwrite", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_KP_Insert, 0,
				"toggle_overwrite", 0);
}

static void
eel_editable_label_editable_init (GtkEditableInterface *iface)
{
  iface->do_insert_text = editable_insert_text_emit;
  iface->do_delete_text = editable_delete_text_emit;
  iface->insert_text = editable_insert_text;
  iface->delete_text = editable_delete_text;
  iface->get_chars = editable_get_chars;
  iface->set_selection_bounds = editable_set_selection_bounds;
  iface->get_selection_bounds = editable_get_selection_bounds;
  iface->set_position = editable_real_set_position;
  iface->get_position = editable_get_position;
}


static void 
eel_editable_label_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
  EelEditableLabel *label;

  label = EEL_EDITABLE_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_TEXT:
      eel_editable_label_set_text (label, g_value_get_string (value));
      break;
    case PROP_JUSTIFY:
      eel_editable_label_set_justify (label, g_value_get_enum (value));
      break;
    case PROP_WRAP:
      eel_editable_label_set_line_wrap (label, g_value_get_boolean (value));
      break;	  
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
eel_editable_label_get_property (GObject     *object,
				 guint        prop_id,
				 GValue      *value,
				 GParamSpec  *pspec)
{
  EelEditableLabel *label;
  gint offset;
  
  label = EEL_EDITABLE_LABEL (object);
  
  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, label->text);
      break;
    case PROP_JUSTIFY:
      g_value_set_enum (value, label->jtype);
      break;
    case PROP_WRAP:
      g_value_set_boolean (value, label->wrap);
      break;
    case PROP_CURSOR_POSITION:
      offset = g_utf8_pointer_to_offset (label->text,
					 label->text + label->selection_end);
      g_value_set_int (value, offset);
      break;
    case PROP_SELECTION_BOUND:
      offset = g_utf8_pointer_to_offset (label->text,
					 label->text + label->selection_anchor);
      g_value_set_int (value, offset);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
eel_editable_label_init (EelEditableLabel *label)
{
  label->jtype = GTK_JUSTIFY_LEFT;
  label->wrap = FALSE;
  label->wrap_mode = PANGO_WRAP_WORD;

  label->layout = NULL;
  label->text_size = 1;
  label->text = g_malloc (label->text_size);
  label->text[0] = '\0';
  label->n_bytes = 0;
  
  gtk_widget_set_can_focus (GTK_WIDGET (label), TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (label)),
                               GTK_STYLE_CLASS_ENTRY);

    /* This object is completely private. No external entity can gain a reference
   * to it; so we create it here and destroy it in finalize().
   */
  label->im_context = gtk_im_multicontext_new ();

  g_signal_connect (G_OBJECT (label->im_context), "commit",
		    G_CALLBACK (eel_editable_label_commit_cb), label);
  g_signal_connect (G_OBJECT (label->im_context), "preedit_changed",
		    G_CALLBACK (eel_editable_label_preedit_changed_cb), label);
  g_signal_connect (G_OBJECT (label->im_context), "retrieve_surrounding",
		    G_CALLBACK (eel_editable_label_retrieve_surrounding_cb), label);
  g_signal_connect (G_OBJECT (label->im_context), "delete_surrounding",
		    G_CALLBACK (eel_editable_label_delete_surrounding_cb), label);
}

/**
 * eel_editable_label_new:
 * @str: The text of the label
 *
 * Creates a new label with the given text inside it. You can
 * pass %NULL to get an empty label widget.
 *
 * Return value: the new #EelEditableLabel
 **/
GtkWidget*
eel_editable_label_new (const gchar *str)
{
  EelEditableLabel *label;
  
  label = g_object_new (EEL_TYPE_EDITABLE_LABEL, NULL);

  if (str && *str)
    eel_editable_label_set_text (label, str);
  
  return GTK_WIDGET (label);
}

/**
 * eel_editable_label_set_text:
 * @label: a #EelEditableLabel
 * @str: The text you want to set.
 *
 * Sets the text within the #EelEditableLabel widget.  It overwrites any text that
 * was there before.  
 *
 * This will also clear any previously set mnemonic accelerators.
 **/
void
eel_editable_label_set_text (EelEditableLabel *label,
			     const gchar *str)
{
  GtkEditable *editable;
  int tmp_pos;
  
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  g_return_if_fail (str != NULL);
  
  if (strcmp (label->text, str) == 0)
    return;

  editable = GTK_EDITABLE (label);
  gtk_editable_delete_text (editable, 0, -1);
  tmp_pos = 0;
  gtk_editable_insert_text (editable, str, strlen (str), &tmp_pos);
}

/**
 * eel_editable_label_get_text:
 * @label: a #EelEditableLabel
 * 
 * Fetches the text from a label widget, as displayed on the
 * screen. This does not include any embedded underlines
 * indicating mnemonics or Pango markup. (See eel_editable_label_get_label())
 * 
 * Return value: the text in the label widget. This is the internal
 *   string used by the label, and must not be modified.
 **/
const gchar *
eel_editable_label_get_text (EelEditableLabel *label)
{
  g_return_val_if_fail (EEL_IS_EDITABLE_LABEL (label), NULL);

  return label->text;
}

/**
 * eel_editable_label_set_justify:
 * @label: a #EelEditableLabel
 * @jtype: a #GtkJustification
 *
 * Sets the alignment of the lines in the text of the label relative to
 * each other.  %GTK_JUSTIFY_LEFT is the default value when the
 * widget is first created with eel_editable_label_new(). If you instead want
 * to set the alignment of the label as a whole, use
 * gtk_misc_set_alignment() instead. eel_editable_label_set_justify() has no
 * effect on labels containing only a single line.
 **/
void
eel_editable_label_set_justify (EelEditableLabel        *label,
				GtkJustification jtype)
{
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  g_return_if_fail (jtype >= GTK_JUSTIFY_LEFT && jtype <= GTK_JUSTIFY_FILL);
  
  if ((GtkJustification) label->jtype != jtype)
    {
      label->jtype = jtype;

      /* No real need to be this drastic, but easier than duplicating the code */
      eel_editable_label_recompute (label);
      
      g_object_notify (G_OBJECT (label), "justify");
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}

/**
 * eel_editable_label_get_justify:
 * @label: a #EelEditableLabel
 *
 * Returns the justification of the label. See eel_editable_label_set_justify ().
 *
 * Return value: #GtkJustification
 **/
GtkJustification
eel_editable_label_get_justify (EelEditableLabel *label)
{
  g_return_val_if_fail (EEL_IS_EDITABLE_LABEL (label), 0);

  return label->jtype;
}

void
eel_editable_label_set_draw_outline (EelEditableLabel *label,
				     gboolean          draw_outline)
{
    draw_outline = draw_outline != FALSE;

    if (label->draw_outline != draw_outline)
    {
      label->draw_outline = draw_outline;
      
      gtk_widget_queue_draw (GTK_WIDGET (label));
    }

}


/**
 * eel_editable_label_set_line_wrap:
 * @label: a #EelEditableLabel
 * @wrap: the setting
 *
 * Toggles line wrapping within the #EelEditableLabel widget.  %TRUE makes it break
 * lines if text exceeds the widget's size.  %FALSE lets the text get cut off
 * by the edge of the widget if it exceeds the widget size.
 **/
void
eel_editable_label_set_line_wrap (EelEditableLabel *label,
				  gboolean  wrap)
{
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  
  wrap = wrap != FALSE;
  
  if (label->wrap != wrap)
    {
      label->wrap = wrap;
      g_object_notify (G_OBJECT (label), "wrap");
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
}


void
eel_editable_label_set_line_wrap_mode (EelEditableLabel *label,
				       PangoWrapMode     mode)
{
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  
  if (label->wrap_mode != mode)
    {
      label->wrap_mode = mode;
      
      gtk_widget_queue_resize (GTK_WIDGET (label));
    }
  
}


/**
 * eel_editable_label_get_line_wrap:
 * @label: a #EelEditableLabel
 *
 * Returns whether lines in the label are automatically wrapped. See eel_editable_label_set_line_wrap ().
 *
 * Return value: %TRUE if the lines of the label are automatically wrapped.
 */
gboolean
eel_editable_label_get_line_wrap (EelEditableLabel *label)
{
  g_return_val_if_fail (EEL_IS_EDITABLE_LABEL (label), FALSE);

  return label->wrap;
}

PangoFontDescription *
eel_editable_label_get_font_description (EelEditableLabel *label)
{
  if (label->font_desc)
    return pango_font_description_copy (label->font_desc);

  return NULL;
}

void
eel_editable_label_set_font_description (EelEditableLabel *label,
					 const PangoFontDescription *desc)
{
  if (label->font_desc)
    pango_font_description_free (label->font_desc);

  if (desc)
    label->font_desc = pango_font_description_copy (desc);
  else
    label->font_desc = NULL;

  eel_editable_label_clear_layout (label);
}

static void
eel_editable_label_finalize (GObject *object)
{
  EelEditableLabel *label;
  
  g_assert (EEL_IS_EDITABLE_LABEL (object));
  
  label = EEL_EDITABLE_LABEL (object);

  if (label->font_desc)
    {
      pango_font_description_free (label->font_desc);
      label->font_desc = NULL;
    }
  
  g_object_unref (G_OBJECT (label->im_context));
  label->im_context = NULL;
  
  g_free (label->text);
  label->text = NULL;

  if (label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }

  G_OBJECT_CLASS (eel_editable_label_parent_class)->finalize (object);
}

static void
eel_editable_label_clear_layout (EelEditableLabel *label)
{
  if (label->layout)
    {
      g_object_unref (G_OBJECT (label->layout));
      label->layout = NULL;
    }
}

static void
eel_editable_label_recompute (EelEditableLabel *label)
{
  eel_editable_label_clear_layout (label);
  eel_editable_label_check_cursor_blink (label);
}

typedef struct _LabelWrapWidth LabelWrapWidth;
struct _LabelWrapWidth
{
  gint width;
  PangoFontDescription *font_desc;
};

static void
label_wrap_width_free (gpointer data)
{
  LabelWrapWidth *wrap_width = data;
  pango_font_description_free (wrap_width->font_desc);
  g_free (wrap_width);
}

static gint
get_label_wrap_width (EelEditableLabel *label)
{
  PangoLayout *layout;
  GtkStyleContext *style = gtk_widget_get_style_context (GTK_WIDGET (label));
  PangoFontDescription *desc;

  LabelWrapWidth *wrap_width = g_object_get_data (G_OBJECT (style), "gtk-label-wrap-width");
  if (!wrap_width)
    {
      wrap_width = g_new0 (LabelWrapWidth, 1);
      g_object_set_data_full (G_OBJECT (style), "gtk-label-wrap-width",
			      wrap_width, label_wrap_width_free);
    }

  gtk_style_context_get (style, gtk_widget_get_state_flags (GTK_WIDGET (label)),
                         GTK_STYLE_PROPERTY_FONT, &desc,
                         NULL);

  if (wrap_width->font_desc && pango_font_description_equal (wrap_width->font_desc, desc))
    goto out;

  if (wrap_width->font_desc)
    pango_font_description_free (wrap_width->font_desc);

  wrap_width->font_desc = pango_font_description_copy (desc);

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (label), 
					   "This long string gives a good enough length for any line to have.");
  pango_layout_get_size (layout, &wrap_width->width, NULL);
  g_object_unref (layout);

 out:
  pango_font_description_free (desc);

  return wrap_width->width;
}

static void
eel_editable_label_ensure_layout (EelEditableLabel *label,
				  gboolean        include_preedit)
{
  GtkWidget *widget;
  PangoRectangle logical_rect;

  /* Normalize for comparisons */
  include_preedit = include_preedit != 0;

  if (label->preedit_length > 0 &&
      include_preedit != label->layout_includes_preedit)
    eel_editable_label_clear_layout (label);
  
  widget = GTK_WIDGET (label);

  if (label->layout == NULL)
    {
      gchar *preedit_string = NULL;
      gint preedit_length = 0;
      PangoAttrList *preedit_attrs = NULL;
      PangoAlignment align = PANGO_ALIGN_LEFT; /* Quiet gcc */
      PangoAttrList *tmp_attrs = pango_attr_list_new ();

      if (include_preedit)
	{
	  gtk_im_context_get_preedit_string (label->im_context,
					     &preedit_string, &preedit_attrs, NULL);
	  preedit_length = label->preedit_length;
	}

      if (preedit_length)
	{
	  GString *tmp_string = g_string_new (NULL);
	  
	  g_string_prepend_len (tmp_string, label->text, label->n_bytes);
	  g_string_insert (tmp_string, label->selection_anchor, preedit_string);
      
	  label->layout = gtk_widget_create_pango_layout (widget, tmp_string->str);
      
	  pango_attr_list_splice (tmp_attrs, preedit_attrs,
				  label->selection_anchor, preedit_length);
	  
	  g_string_free (tmp_string, TRUE);
	}
      else
	{
	  label->layout = gtk_widget_create_pango_layout (widget, label->text);
	}
      label->layout_includes_preedit = include_preedit;

      if (label->font_desc != NULL)
	pango_layout_set_font_description (label->layout, label->font_desc);
      
      pango_layout_set_attributes (label->layout, tmp_attrs);
      
      if (preedit_string)
	g_free (preedit_string);
      if (preedit_attrs)
	pango_attr_list_unref (preedit_attrs);
      pango_attr_list_unref (tmp_attrs);

      switch (label->jtype)
	{
	case GTK_JUSTIFY_LEFT:
	  align = PANGO_ALIGN_LEFT;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  align = PANGO_ALIGN_RIGHT;
	  break;
	case GTK_JUSTIFY_CENTER:
	  align = PANGO_ALIGN_CENTER;
	  break;
	case GTK_JUSTIFY_FILL:
	  /* FIXME: This just doesn't work to do this */
	  align = PANGO_ALIGN_LEFT;
	  pango_layout_set_justify (label->layout, TRUE);
	  break;
	default:
	  g_assert_not_reached();
	}

      pango_layout_set_alignment (label->layout, align);

      if (label->wrap)
	{
	  gint longest_paragraph;
	  gint width, height;
	  gint set_width;

	  gtk_widget_get_size_request (widget, &set_width, NULL);
	  if (set_width > 0)
	    pango_layout_set_width (label->layout, set_width * PANGO_SCALE);
	  else
	    {
	      gint wrap_width;
	      
	      pango_layout_set_width (label->layout, -1);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);

	      width = logical_rect.width;
	      
	      /* Try to guess a reasonable maximum width */
	      longest_paragraph = width;

	      wrap_width = get_label_wrap_width (label);
	      width = MIN (width, wrap_width);
	      width = MIN (width,
			   PANGO_SCALE * (gdk_screen_width () + 1) / 2);
	      
	      pango_layout_set_width (label->layout, width);
	      pango_layout_get_extents (label->layout, NULL, &logical_rect);
	      width = logical_rect.width;
	      height = logical_rect.height;
	      
	      /* Unfortunately, the above may leave us with a very unbalanced looking paragraph,
	       * so we try short search for a narrower width that leaves us with the same height
	       */
	      if (longest_paragraph > 0)
		{
		  gint nlines, perfect_width;
		  
		  nlines = pango_layout_get_line_count (label->layout);
		  perfect_width = (longest_paragraph + nlines - 1) / nlines;
		  
		  if (perfect_width < width)
		    {
		      pango_layout_set_width (label->layout, perfect_width);
		      pango_layout_get_extents (label->layout, NULL, &logical_rect);
		      
		      if (logical_rect.height <= height)
			width = logical_rect.width;
		      else
			{
			  gint mid_width = (perfect_width + width) / 2;
			  
			  if (mid_width > perfect_width)
			    {
			      pango_layout_set_width (label->layout, mid_width);
			      pango_layout_get_extents (label->layout, NULL, &logical_rect);
			      
			      if (logical_rect.height <= height)
				width = logical_rect.width;
			    }
			}
		    }
		}
	      pango_layout_set_width (label->layout, width);
	    }
	  pango_layout_set_wrap (label->layout, label->wrap_mode);
	}
      else		/* !label->wrap */
	pango_layout_set_width (label->layout, -1);
    }
}

static void
eel_editable_label_size_request (GtkWidget      *widget,
				 GtkRequisition *requisition)
{
  EelEditableLabel *label;
  gint width, height;
  PangoRectangle logical_rect;
  gint set_width;
  gint xpad, ypad;

  g_assert (EEL_IS_EDITABLE_LABEL (widget));
  g_assert (requisition != NULL);
  
  label = EEL_EDITABLE_LABEL (widget);

  /*  
   * If word wrapping is on, then the height requisition can depend
   * on:
   *
   *   - Any width set on the widget via gtk_widget_set_size_request().
   *   - The padding of the widget (xpad, set by gtk_misc_set_padding)
   *
   * Instead of trying to detect changes to these quantities, if we
   * are wrapping, we just rewrap for each size request. Since
   * size requisitions are cached by the GTK+ core, this is not
   * expensive.
   */

  if (label->wrap)
    eel_editable_label_recompute (label);

  eel_editable_label_ensure_layout (label, TRUE);

  gtk_misc_get_padding (&label->misc,
                        &xpad, &ypad);
  width = xpad * 2;
  height = ypad * 2;

  pango_layout_get_extents (label->layout, NULL, &logical_rect);
  
  gtk_widget_get_size_request (widget, &set_width, NULL);
  if (label->wrap && set_width > 0)
    width += set_width;
  else 
    width += PANGO_PIXELS (logical_rect.width);
  
  height += PANGO_PIXELS (logical_rect.height);

  requisition->width = width;
  requisition->height = height;
}

static void
eel_editable_label_get_preferred_width (GtkWidget *widget,
                                        gint      *minimum,
                                        gint      *natural)
{
  GtkRequisition requisition;

  eel_editable_label_size_request (widget, &requisition);

  *minimum = *natural = requisition.width;
}

static void
eel_editable_label_get_preferred_height (GtkWidget *widget,
                                         gint      *minimum,
                                         gint      *natural)
{
  GtkRequisition requisition;

  eel_editable_label_size_request (widget, &requisition);

  *minimum = *natural = requisition.height;
}

static void
eel_editable_label_size_allocate (GtkWidget     *widget,
				  GtkAllocation *allocation)
{
  (* GTK_WIDGET_CLASS (eel_editable_label_parent_class)->size_allocate) (widget, allocation);
}

static void
eel_editable_label_state_changed (GtkWidget   *widget,
				  GtkStateType prev_state)
{
  EelEditableLabel *label;
  
  label = EEL_EDITABLE_LABEL (widget);

  /* clear any selection if we're insensitive */
  if (!gtk_widget_is_sensitive (widget))
    eel_editable_label_select_region (label, 0, 0);

  if (GTK_WIDGET_CLASS (eel_editable_label_parent_class)->state_changed)
    GTK_WIDGET_CLASS (eel_editable_label_parent_class)->state_changed (widget, prev_state);
}

static void 
eel_editable_label_style_updated (GtkWidget *widget)
{
  EelEditableLabel *label;

  g_assert (EEL_IS_EDITABLE_LABEL (widget));
  
  label = EEL_EDITABLE_LABEL (widget);

  GTK_WIDGET_CLASS (eel_editable_label_parent_class)->style_updated (widget);

  /* We have to clear the layout, fonts etc. may have changed */
  eel_editable_label_recompute (label);
}

static void 
eel_editable_label_direction_changed (GtkWidget        *widget,
				      GtkTextDirection previous_dir)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (widget);

  if (label->layout)
    pango_layout_context_changed (label->layout);

  GTK_WIDGET_CLASS (eel_editable_label_parent_class)->direction_changed (widget, previous_dir);
}

static void
get_layout_location (EelEditableLabel  *label,
                     gint      *xp,
                     gint      *yp)
{
  GtkMisc *misc;
  GtkWidget *widget;
  gfloat xalign, yalign;
  GtkRequisition req;
  gint x, y, xpad, ypad;
  GtkAllocation allocation;
  
  misc = GTK_MISC (label);
  widget = GTK_WIDGET (label);
  gtk_misc_get_alignment (misc, &xalign, &yalign);
  
  if (gtk_widget_get_direction (widget) != GTK_TEXT_DIR_LTR)
    xalign = 1.0 - xalign;

  gtk_widget_get_preferred_size (widget, &req, NULL);
  gtk_misc_get_padding (misc, &xpad, &ypad);

  gtk_widget_get_allocation (widget, &allocation);
  x = floor (xpad
             + ((allocation.width - req.width) * xalign)
             + 0.5);
  
  y = floor (ypad
             + ((allocation.height - req.height) * yalign)
             + 0.5);

  if (xp)
    *xp = x;

  if (yp)
    *yp = y;
}

static gint
eel_editable_label_get_cursor_pos (EelEditableLabel  *label,
				   PangoRectangle *strong_pos,
				   PangoRectangle *weak_pos)
{
  const gchar *text;
  const gchar *preedit_text;
  gint index;
  
  eel_editable_label_ensure_layout (label, TRUE);
  
  text = pango_layout_get_text (label->layout);
  preedit_text = text + label->selection_anchor;
  index = label->selection_anchor +
    g_utf8_offset_to_pointer (preedit_text, label->preedit_cursor) - preedit_text;
      
  pango_layout_get_cursor_pos (label->layout, index, strong_pos, weak_pos);

  return index;
}

/* Copied from gtkutil private function */
static gboolean
eel_editable_label_get_block_cursor_location (EelEditableLabel  *label,
					      gint *index,
					      PangoRectangle *pos,
					      gboolean *at_line_end)
{
  const gchar *text;
  const gchar *preedit_text;
  PangoLayoutLine *layout_line;
  PangoRectangle strong_pos, weak_pos;
  gint line_no;
  gboolean rtl;
  PangoContext *context;
  PangoFontMetrics *metrics;
  const PangoFontDescription *font_desc;
  
  eel_editable_label_ensure_layout (label, TRUE);
  
  text = pango_layout_get_text (label->layout);
  preedit_text = text + label->selection_anchor;
  text = g_utf8_offset_to_pointer (preedit_text, label->preedit_cursor);
  index[0] = label->selection_anchor + text - preedit_text;

  pango_layout_index_to_pos (label->layout, index[0], pos);

  index[1] = label->selection_anchor + g_utf8_next_char (text) - preedit_text;

  if (pos->width != 0)
    {
      if (at_line_end)
	*at_line_end = FALSE;
      if (pos->width < 0) /* RTL char, shift x value back to top left of rect */
	{
	  pos->x += pos->width;
	  pos->width = -pos->width;
	}
      return TRUE;
    }

  pango_layout_index_to_line_x (label->layout, index[0], FALSE, &line_no, NULL);
  layout_line = pango_layout_get_line_readonly (label->layout, line_no);
  if (layout_line == NULL)
    return FALSE;

    text = pango_layout_get_text (label->layout);
  if (index[0] < layout_line->start_index + layout_line->length)
    {
      /* this may be a zero-width character in the middle of the line,
       * or it could be a character where line is wrapped, we do want
       * block cursor in latter case */
      if (g_utf8_next_char (text + index[0]) - text !=
	  layout_line->start_index + layout_line->length)
	{
	  /* zero-width character in the middle of the line, do not
	   * bother with block cursor */
	  return FALSE;
	}
    }

  /* Cursor is at the line end. It may be an empty line, or it could
   * be on the left or on the right depending on text direction, or it
   * even could be in the middle of visual layout in bidi text. */

  pango_layout_get_cursor_pos (label->layout, index[0], &strong_pos, &weak_pos);

  if (strong_pos.x != weak_pos.x)
    {
      /* do not show block cursor in this case, since the character typed
       * in may or may not appear at the cursor position */
      return FALSE;
    }

  context = pango_layout_get_context (label->layout);

  /* In case when index points to the end of line, pos->x is always most right
   * pixel of the layout line, so we need to correct it for RTL text. */
  if (layout_line->length)
    {
      if (layout_line->resolved_dir == PANGO_DIRECTION_RTL)
	{
	  PangoLayoutIter *iter;
	  PangoRectangle line_rect;
	  gint i;
	  gint left, right;
	  const gchar *p;

	  p = g_utf8_prev_char (text + index[0]);

	  pango_layout_line_index_to_x (layout_line, p - text, FALSE, &left);
	  pango_layout_line_index_to_x (layout_line, p - text, TRUE, &right);
	  pos->x = MIN (left, right);

	  iter = pango_layout_get_iter (label->layout);
	  for (i = 0; i < line_no; i++)
	    pango_layout_iter_next_line (iter);
	  pango_layout_iter_get_line_extents (iter, NULL, &line_rect);
	  pango_layout_iter_free (iter);

          rtl = TRUE;
	  pos->x += line_rect.x;
	}
      else
	rtl = FALSE;
    }
  else
    {
      rtl = pango_context_get_base_dir (context) == PANGO_DIRECTION_RTL;
    }
  
  font_desc = pango_layout_get_font_description (label->layout);
  if (!font_desc)
    font_desc = pango_context_get_font_description (context);

  metrics = pango_context_get_metrics (context, font_desc, NULL);
  pos->width = pango_font_metrics_get_approximate_char_width (metrics);
  pango_font_metrics_unref (metrics);

  if (rtl)
    pos->x -= pos->width - 1;

  if (at_line_end)
    *at_line_end = TRUE;

  return pos->width != 0;
}


/* These functions are copies from gtk+, as they are not exported from gtk+ */

static void
eel_editable_label_draw_cursor (EelEditableLabel  *label, cairo_t *cr, gint xoffset, gint yoffset)
{
  if (gtk_widget_is_drawable (GTK_WIDGET (label)))
    {
      GtkWidget *widget = GTK_WIDGET (label);

      gboolean block;
      gboolean block_at_line_end;
      gint range[2];
      gint index;
      GtkStyleContext *context;
      PangoRectangle strong_pos;

      context = gtk_widget_get_style_context (widget);
      index = eel_editable_label_get_cursor_pos (label, NULL, NULL);

      if (label->overwrite_mode &&
	  eel_editable_label_get_block_cursor_location (label, range,
							&strong_pos,
							&block_at_line_end))
	block = TRUE;
      else
	block = FALSE;

      if (!block)
	{
          gtk_render_insertion_cursor (context, cr,
                                       xoffset, yoffset,
                                       label->layout, index,
                                       gdk_keymap_get_direction (gdk_keymap_get_default ()));
	}
      else /* Block cursor */
	{
          GdkRGBA fg_color;
	  cairo_region_t *clip;

          gtk_style_context_get_color (context, GTK_STATE_FLAG_NORMAL, &fg_color);

	  cairo_save (cr);
          gdk_cairo_set_source_rgba (cr, &fg_color);

	  cairo_rectangle (cr,
			   xoffset + PANGO_PIXELS (strong_pos.x),
			   yoffset + PANGO_PIXELS (strong_pos.y),
			   PANGO_PIXELS (strong_pos.width),
			   PANGO_PIXELS (strong_pos.height));
	  cairo_fill (cr);

	  if (!block_at_line_end)
	    {
              GdkRGBA color;

	      clip = gdk_pango_layout_get_clip_region (label->layout,
						       xoffset, yoffset,
						       range, 1);

	      gdk_cairo_region (cr, clip);
	      cairo_clip (cr);

              gtk_style_context_get_background_color (context, GTK_STATE_FLAG_FOCUSED,
                                                      &color);

	      gdk_cairo_set_source_rgba (cr,
                                         &color);
	      cairo_move_to (cr, xoffset, yoffset);
	      pango_cairo_show_layout (cr, label->layout);

	      cairo_region_destroy (clip);
	    }

	  cairo_restore (cr);
	}
    }
}


static gint
eel_editable_label_draw (GtkWidget *widget,
                         cairo_t   *cr)
{
  EelEditableLabel *label;
  GtkStyleContext *style;
  gint x, y;
  
  g_assert (EEL_IS_EDITABLE_LABEL (widget));
  
  label = EEL_EDITABLE_LABEL (widget);
  style = gtk_widget_get_style_context (widget);

  eel_editable_label_ensure_layout (label, TRUE);
  
  if (gtk_widget_get_visible (widget) && gtk_widget_get_mapped (widget) &&
      label->text)
    {
      get_layout_location (label, &x, &y);
      
      gtk_render_layout (style,
                         cr,
                         x, y,
                         label->layout);

      if (label->selection_anchor != label->selection_end)
        {
          gint range[2];
	  const char *text;
          cairo_region_t *clip;
	  GtkStateType state;
          GdkRGBA background_color;

          range[0] = label->selection_anchor;
          range[1] = label->selection_end;

	  /* Handle possible preedit string */
	  if (label->preedit_length > 0 &&
	      range[1] > label->selection_anchor)
	    {
	      text = pango_layout_get_text (label->layout) + label->selection_anchor;
	      range[1] += g_utf8_offset_to_pointer (text, label->preedit_length) - text;
	    }
	  
          if (range[0] > range[1])
            {
              gint tmp = range[0];
              range[0] = range[1];
              range[1] = tmp;
            }

          clip = gdk_pango_layout_get_clip_region (label->layout,
                                                   x, y,
                                                   range,
                                                   1);

          cairo_save (cr);

	  gdk_cairo_region (cr, clip);
	  cairo_clip (cr);

          state = gtk_widget_get_state_flags (widget);
	  state |= GTK_STATE_FLAG_SELECTED;

          gtk_style_context_get_background_color (style, state, &background_color);
	  gdk_cairo_set_source_rgba (cr, &background_color);
	  cairo_paint (cr);

          gtk_style_context_save (style);
          gtk_style_context_set_state (style, state);

          gtk_render_layout (style, cr,
                             x, y, label->layout);

          gtk_style_context_restore (style);
          cairo_restore (cr);

          cairo_region_destroy (clip);
        }
      else if (gtk_widget_has_focus (widget))
	eel_editable_label_draw_cursor (label, cr, x, y);

      if (label->draw_outline) {
        gtk_style_context_save (style);
        gtk_style_context_set_state (style, gtk_widget_get_state_flags (widget));

        gtk_render_frame (style, cr,
                          0, 0,
                          gtk_widget_get_allocated_width (widget),
                          gtk_widget_get_allocated_height (widget));

        gtk_style_context_restore (style);
      }
    }

  return FALSE;
}

static void
eel_editable_label_realize (GtkWidget *widget)
{
  EelEditableLabel *label;
  GdkWindowAttr attributes;
  gint attributes_mask;
  GtkAllocation allocation;
  GdkWindow *window;
  GtkStyleContext *style;

  gtk_widget_set_realized (widget, TRUE);
  label = EEL_EDITABLE_LABEL (widget);
  gtk_widget_get_allocation (widget, &allocation);

  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.cursor = gdk_cursor_new (GDK_XTERM);
  attributes.event_mask = gtk_widget_get_events (widget) |
    (GDK_EXPOSURE_MASK |
     GDK_BUTTON_PRESS_MASK |
     GDK_BUTTON_RELEASE_MASK |
     GDK_BUTTON1_MOTION_MASK |
     GDK_BUTTON3_MOTION_MASK |
     GDK_POINTER_MOTION_HINT_MASK |
     GDK_POINTER_MOTION_MASK |
     GDK_ENTER_NOTIFY_MASK |
     GDK_LEAVE_NOTIFY_MASK);

  attributes_mask = GDK_WA_X | GDK_WA_Y  | GDK_WA_VISUAL | GDK_WA_CURSOR;

  window = gdk_window_new (gtk_widget_get_parent_window (widget),
			   &attributes, attributes_mask);
  gtk_widget_set_window (widget, window);
  gdk_window_set_user_data (window, widget);

  g_object_unref (attributes.cursor);

  style = gtk_widget_get_style_context (widget);
  gtk_style_context_set_background (style, gtk_widget_get_window (widget));
  gtk_im_context_set_client_window (label->im_context, gtk_widget_get_window (widget));
}

static void
eel_editable_label_unrealize (GtkWidget *widget)
{
  EelEditableLabel *label;

  label = EEL_EDITABLE_LABEL (widget);

  /* Strange. Copied from GtkEntry, should be NULL? */
  gtk_im_context_set_client_window (label->im_context, NULL);
  
  (* GTK_WIDGET_CLASS (eel_editable_label_parent_class)->unrealize) (widget);
}

static void
eel_editable_label_map (GtkWidget *widget)
{
  (* GTK_WIDGET_CLASS (eel_editable_label_parent_class)->map) (widget);
}

static void
eel_editable_label_unmap (GtkWidget *widget)
{
  (* GTK_WIDGET_CLASS (eel_editable_label_parent_class)->unmap) (widget);
}

static void
window_to_layout_coords (EelEditableLabel *label,
                         gint     *x,
                         gint     *y)
{
  gint lx, ly;

  /* get layout location in gtk_widget_get_window (widget) coords */
  get_layout_location (label, &lx, &ly);
  
  if (x)
    *x -= lx;                   /* go to layout */

  if (y)
    *y -= ly;                   /* go to layout */
}

static void
get_layout_index (EelEditableLabel *label,
                  gint      x,
                  gint      y,
                  gint     *index)
{
  gint trailing = 0;
  const gchar *cluster;
  const gchar *cluster_end;

  *index = 0;
  
  eel_editable_label_ensure_layout (label, TRUE);
  
  window_to_layout_coords (label, &x, &y);

  x *= PANGO_SCALE;
  y *= PANGO_SCALE;
  
  pango_layout_xy_to_index (label->layout,
                            x, y,
                            index, &trailing);

  if (*index >= label->selection_anchor && label->preedit_length)
    {
      if (*index >= label->selection_anchor + label->preedit_length)
	*index -= label->preedit_length;
      else
	{
	  *index = label->selection_anchor;
	  trailing = 0;
	}
    }
  
  cluster = label->text + *index;
  cluster_end = cluster;
  while (trailing)
    {
      cluster_end = g_utf8_next_char (cluster_end);
      --trailing;
    }

  *index += (cluster_end - cluster);
}

static void
eel_editable_label_select_word (EelEditableLabel *label)
{
  gint min, max;
  
  gint start_index = eel_editable_label_move_backward_word (label, label->selection_end);
  gint end_index = eel_editable_label_move_forward_word (label, label->selection_end);

  min = MIN (label->selection_anchor,
	     label->selection_end);
  max = MAX (label->selection_anchor,
	     label->selection_end);

  min = MIN (min, start_index);
  max = MAX (max, end_index);

  eel_editable_label_select_region_index (label, min, max);
}

static gint
eel_editable_label_button_press (GtkWidget      *widget,
				 GdkEventButton *event)
{
  EelEditableLabel *label;
  gint index = 0;
  
  label = EEL_EDITABLE_LABEL (widget);

  if (event->button == 1)
    {
      if (!gtk_widget_has_focus (widget))
	gtk_widget_grab_focus (widget);

      if (event->type == GDK_3BUTTON_PRESS)
	{
	  eel_editable_label_select_region_index (label, 0, strlen (label->text));
	  return TRUE;
	}
      
      if (event->type == GDK_2BUTTON_PRESS)
	{
	  eel_editable_label_select_word (label);
	  return TRUE;
	}
      
      get_layout_index (label, event->x, event->y, &index);
      
      if ((label->selection_anchor !=
	   label->selection_end) &&
	  (event->state & GDK_SHIFT_MASK))
	{
	  gint min, max;
	  
	  /* extend (same as motion) */
	  min = MIN (label->selection_anchor,
		     label->selection_end);
	  max = MAX (label->selection_anchor,
		     label->selection_end);
	  
	  min = MIN (min, index);
	  max = MAX (max, index);
	  
	  /* ensure the anchor is opposite index */
	  if (index == min)
	    {
	      gint tmp = min;
	      min = max;
	      max = tmp;
	    }
	  
	  eel_editable_label_select_region_index (label, min, max);
	}
      else
	{
	  if (event->type == GDK_3BUTTON_PRESS)
	      eel_editable_label_select_region_index (label, 0, strlen (label->text));
	  else if (event->type == GDK_2BUTTON_PRESS)
	      eel_editable_label_select_word (label);
	  else 
	    /* start a replacement */
	    eel_editable_label_select_region_index (label, index, index);
	}
  
      return TRUE;
    }
  else if (event->button == 2 && event->type == GDK_BUTTON_PRESS)
    {
      get_layout_index (label, event->x, event->y, &index);
      
      eel_editable_label_select_region_index (label, index, index);
      eel_editable_label_paste (label, GDK_SELECTION_PRIMARY);
      
      return TRUE;
    }
  else if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
      eel_editable_label_do_popup (label, event);

      return TRUE;
      
    }
  return FALSE;
}

static gint
eel_editable_label_button_release (GtkWidget      *widget,
				   GdkEventButton *event)

{
  if (event->button != 1)
    return FALSE;
  
  /* The goal here is to return TRUE iff we ate the
   * button press to start selecting.
   */
  
  return TRUE;
}

static gint
eel_editable_label_motion (GtkWidget      *widget,
			   GdkEventMotion *event)
{
  EelEditableLabel *label;
  gint index;
  gint x, y;
  
  label = EEL_EDITABLE_LABEL (widget);
  
  if ((event->state & GDK_BUTTON1_MASK) == 0)
    return FALSE;

  gdk_window_get_device_position (gtk_widget_get_window (widget),
                                  event->device,
                                  &x, &y, NULL);

  get_layout_index (label, x, y, &index);

  eel_editable_label_select_region_index (label,
					  label->selection_anchor,
					  index);
  
  return TRUE;
}

static void
get_text_callback (GtkClipboard     *clipboard,
                   GtkSelectionData *selection_data,
                   guint             info,
                   gpointer          user_data_or_owner)
{
  EelEditableLabel *label;
  
  label = EEL_EDITABLE_LABEL (user_data_or_owner);
  
  if ((label->selection_anchor != label->selection_end) &&
      label->text)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->selection_anchor,
                   label->selection_end);
      end = MAX (label->selection_anchor,
                 label->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      gtk_selection_data_set_text (selection_data,
				   label->text + start,
				   end - start);
    }
}

static void
clear_text_callback (GtkClipboard     *clipboard,
                     gpointer          user_data_or_owner)
{
  EelEditableLabel *label;

  label = EEL_EDITABLE_LABEL (user_data_or_owner);

  label->selection_anchor = label->selection_end;
      
  gtk_widget_queue_draw (GTK_WIDGET (label));
}

static void
eel_editable_label_select_region_index (EelEditableLabel *label,
					gint      anchor_index,
					gint      end_index)
{
  GtkClipboard *clipboard;

  g_assert (EEL_IS_EDITABLE_LABEL (label));
  

  if (label->selection_anchor == anchor_index &&
      label->selection_end == end_index)
    return;

  eel_editable_label_reset_im_context (label);

  label->selection_anchor = anchor_index;
  label->selection_end = end_index;

  clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);      
      
  if (anchor_index != end_index)
    {
      GtkTargetList *list;
      GtkTargetEntry *targets;
      gint n_targets;

      list = gtk_target_list_new (NULL, 0);
      gtk_target_list_add_text_targets (list, 0);
      targets = gtk_target_table_new_from_list (list, &n_targets);

      gtk_clipboard_set_with_owner (clipboard,
				    targets, n_targets,
				    get_text_callback,
				    clear_text_callback,
				    G_OBJECT (label));

      gtk_clipboard_set_can_store (clipboard, NULL, 0);
      gtk_target_table_free (targets, n_targets);
      gtk_target_list_unref (list);
    }
  else
    {
      if (gtk_clipboard_get_owner (clipboard) == G_OBJECT (label))
	gtk_clipboard_clear (clipboard);
    }
  
  gtk_widget_queue_draw (GTK_WIDGET (label));
  
  g_object_freeze_notify (G_OBJECT (label));
  g_object_notify (G_OBJECT (label), "cursor_position");
  g_object_notify (G_OBJECT (label), "selection_bound");
  g_object_thaw_notify (G_OBJECT (label));
}

/**
 * eel_editable_label_select_region:
 * @label: a #EelEditableLabel
 * @start_offset: start offset (in characters not bytes)
 * @end_offset: end offset (in characters not bytes)
 *
 * Selects a range of characters in the label, if the label is selectable.
 * See eel_editable_label_set_selectable(). If the label is not selectable,
 * this function has no effect. If @start_offset or
 * @end_offset are -1, then the end of the label will be substituted.
 * 
 **/
void
eel_editable_label_select_region  (EelEditableLabel *label,
				   gint      start_offset,
				   gint      end_offset)
{
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  
  if (label->text)
    {
      if (start_offset < 0)
        start_offset = g_utf8_strlen (label->text, -1);
      
      if (end_offset < 0)
        end_offset = g_utf8_strlen (label->text, -1);
      
      eel_editable_label_select_region_index (label,
					      g_utf8_offset_to_pointer (label->text, start_offset) - label->text,
					      g_utf8_offset_to_pointer (label->text, end_offset) - label->text);
    }
}

/**
 * eel_editable_label_get_selection_bounds:
 * @label: a #EelEditableLabel
 * @start: return location for start of selection, as a character offset
 * @end: return location for end of selection, as a character offset
 * 
 * Gets the selected range of characters in the label, returning %TRUE
 * if there's a selection.
 * 
 * Return value: %TRUE if selection is non-empty
 **/
gboolean
eel_editable_label_get_selection_bounds (EelEditableLabel  *label,
					 gint      *start,
					 gint      *end)
{
  gint start_index, end_index;
  gint start_offset, end_offset;
  gint len;
  
  g_return_val_if_fail (EEL_IS_EDITABLE_LABEL (label), FALSE);

      
  start_index = MIN (label->selection_anchor,
		     label->selection_end);
  end_index = MAX (label->selection_anchor,
		   label->selection_end);

  len = strlen (label->text);
  
  if (end_index > len)
    end_index = len;
  
  if (start_index > len)
    start_index = len;
  
  start_offset = g_utf8_strlen (label->text, start_index);
  end_offset = g_utf8_strlen (label->text, end_index);
  
  if (start_offset > end_offset)
    {
      gint tmp = start_offset;
      start_offset = end_offset;
      end_offset = tmp;
    }
  
  if (start)
    *start = start_offset;
  
  if (end)
    *end = end_offset;
  
  return start_offset != end_offset;
}


/**
 * eel_editable_label_get_layout:
 * @label: a #EelEditableLabel
 * 
 * Gets the #PangoLayout used to display the label.
 * The layout is useful to e.g. convert text positions to
 * pixel positions, in combination with eel_editable_label_get_layout_offsets().
 * The returned layout is owned by the label so need not be
 * freed by the caller.
 * 
 * Return value: the #PangoLayout for this label
 **/
PangoLayout*
eel_editable_label_get_layout (EelEditableLabel *label)
{
  g_return_val_if_fail (EEL_IS_EDITABLE_LABEL (label), NULL);

  eel_editable_label_ensure_layout (label, TRUE);

  return label->layout;
}

/**
 * eel_editable_label_get_layout_offsets:
 * @label: a #EelEditableLabel
 * @x: location to store X offset of layout, or %NULL
 * @y: location to store Y offset of layout, or %NULL
 *
 * Obtains the coordinates where the label will draw the #PangoLayout
 * representing the text in the label; useful to convert mouse events
 * into coordinates inside the #PangoLayout, e.g. to take some action
 * if some part of the label is clicked. Of course you will need to
 * create a #GtkEventBox to receive the events, and pack the label
 * inside it, since labels are a #GTK_NO_WINDOW widget. Remember
 * when using the #PangoLayout functions you need to convert to
 * and from pixels using PANGO_PIXELS() or #PANGO_SCALE.
 * 
 **/
void
eel_editable_label_get_layout_offsets (EelEditableLabel *label,
				       gint     *x,
				       gint     *y)
{
  g_return_if_fail (EEL_IS_EDITABLE_LABEL (label));
  
  get_layout_location (label, x, y);
}

static void
eel_editable_label_pend_cursor_blink (EelEditableLabel *label)
{
  /* TODO */
}

static void
eel_editable_label_check_cursor_blink (EelEditableLabel *label)
{
  /* TODO */
}

static gint
eel_editable_label_key_press (GtkWidget   *widget,
			      GdkEventKey *event)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (widget);

  eel_editable_label_pend_cursor_blink (label);

  if (gtk_im_context_filter_keypress (label->im_context, event))
    {
      /*TODO eel_editable_label_obscure_mouse_cursor (label);*/
      label->need_im_reset = TRUE;
      return TRUE;
    }

  if (GTK_WIDGET_CLASS (eel_editable_label_parent_class)->key_press_event (widget, event))
    /* Activate key bindings
     */
    return TRUE;

  return FALSE;
}

static gint
eel_editable_label_key_release (GtkWidget   *widget,
				GdkEventKey *event)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (widget);

  if (gtk_im_context_filter_keypress (label->im_context, event))
    {
      label->need_im_reset = TRUE;
      return TRUE;
    }

  return GTK_WIDGET_CLASS (eel_editable_label_parent_class)->key_release_event (widget, event);
}

static void
eel_editable_label_keymap_direction_changed (GdkKeymap *keymap,
					     EelEditableLabel  *label)
{
  gtk_widget_queue_draw (GTK_WIDGET (label));
}

static gint
eel_editable_label_focus_in (GtkWidget     *widget,
			     GdkEventFocus *event)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (widget);
  
  gtk_widget_queue_draw (widget);
  
  label->need_im_reset = TRUE;
  gtk_im_context_focus_in (label->im_context);

  g_signal_connect (gdk_keymap_get_default (),
		    "direction_changed",
		    G_CALLBACK (eel_editable_label_keymap_direction_changed), label);

  eel_editable_label_check_cursor_blink (label);

  return FALSE;
}

static gint
eel_editable_label_focus_out (GtkWidget     *widget,
			      GdkEventFocus *event)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (widget);
  
  gtk_widget_queue_draw (widget);

  label->need_im_reset = TRUE;
  gtk_im_context_focus_out (label->im_context);

  eel_editable_label_check_cursor_blink (label);
  
  g_signal_handlers_disconnect_by_func (gdk_keymap_get_default (),
                                        (gpointer) eel_editable_label_keymap_direction_changed,
                                        label);
  
  return FALSE;
}

static void
eel_editable_label_delete_text (EelEditableLabel *label,
				int start_pos,
				int end_pos)
{
  int anchor, end;
  
  if (start_pos < 0)
    start_pos = 0;
  if (end_pos < 0 || end_pos > label->n_bytes)
    end_pos = label->n_bytes;
  
  if (start_pos < end_pos)
    {
      g_memmove (label->text + start_pos, label->text + end_pos, label->n_bytes + 1 - end_pos);
      label->n_bytes -= (end_pos - start_pos);

      anchor = label->selection_anchor;
      if (anchor > start_pos)
	anchor -= MIN (anchor, end_pos) - start_pos;

      end = label->selection_end;
      if (end > start_pos)
	end -= MIN (end, end_pos) - start_pos;
      
      /* We might have changed the selection */
      eel_editable_label_select_region_index (label, anchor, end);
      
      eel_editable_label_recompute (label);  
      gtk_widget_queue_resize (GTK_WIDGET (label));
      
      g_object_notify (G_OBJECT (label), "text");
      g_signal_emit_by_name (GTK_EDITABLE (label), "changed");
    }
}

static void
eel_editable_label_insert_text (EelEditableLabel *label,
				const gchar *new_text,
				gint         new_text_length,
				gint        *index)
{
  if (new_text_length + label->n_bytes + 1 > label->text_size)
    {
      while (new_text_length + label->n_bytes + 1 > label->text_size)
	{
	  if (label->text_size == 0)
	    label->text_size = 16;
	  else
	    label->text_size *= 2;
	}

      label->text = g_realloc (label->text, label->text_size);
    }

  g_object_freeze_notify (G_OBJECT (label));

  g_memmove (label->text + *index + new_text_length, label->text + *index, label->n_bytes - *index);
  memcpy (label->text + *index, new_text, new_text_length);
  
  label->n_bytes += new_text_length;

  /* NUL terminate for safety and convenience */
  label->text[label->n_bytes] = '\0';

  g_object_notify (G_OBJECT (label), "text");

  if (label->selection_anchor > *index)
    {
      g_object_notify (G_OBJECT (label), "cursor_position");
      g_object_notify (G_OBJECT (label), "selection_bound");
      label->selection_anchor += new_text_length;
    }
  
  if (label->selection_end > *index)
    {
      label->selection_end += new_text_length;
      g_object_notify (G_OBJECT (label), "selection_bound");
    }

  *index += new_text_length;

  eel_editable_label_recompute (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));

  g_object_thaw_notify (G_OBJECT (label));
  g_signal_emit_by_name (GTK_EDITABLE (label), "changed");
}

/* Used for im_commit_cb and inserting Unicode chars */
static void
eel_editable_label_enter_text (EelEditableLabel *label,
			       const gchar    *str)
{
  GtkEditable *editable = GTK_EDITABLE (label);
  gint tmp_pos;
  gboolean old_need_im_reset;

  /* Never reset the im while commiting, as that resets possible im state */
  old_need_im_reset = label->need_im_reset;
  label->need_im_reset = FALSE;

  if (label->selection_end != label->selection_anchor)
    gtk_editable_delete_selection (editable);
  else
    {
      if (label->overwrite_mode)
        eel_editable_label_delete_from_cursor (label, GTK_DELETE_CHARS, 1);
    }
  
  tmp_pos = g_utf8_pointer_to_offset (label->text,
                                      label->text + label->selection_anchor);
  gtk_editable_insert_text (GTK_EDITABLE (label), str, strlen (str), &tmp_pos);
  tmp_pos = g_utf8_offset_to_pointer (label->text, tmp_pos) - label->text;
  eel_editable_label_select_region_index (label, tmp_pos, tmp_pos);

  label->need_im_reset = old_need_im_reset;
}

/* IM Context Callbacks
 */

static void
eel_editable_label_commit_cb (GtkIMContext *context,
			      const gchar  *str,
			      EelEditableLabel  *label)
{
  eel_editable_label_enter_text (label, str);
}

static void 
eel_editable_label_preedit_changed_cb (GtkIMContext *context,
				       EelEditableLabel  *label)
{
  gchar *preedit_string;
  gint cursor_pos;
  
  gtk_im_context_get_preedit_string (label->im_context,
				     &preedit_string, NULL,
				     &cursor_pos);
  label->preedit_length = strlen (preedit_string);
  cursor_pos = CLAMP (cursor_pos, 0, g_utf8_strlen (preedit_string, -1));
  label->preedit_cursor = cursor_pos;
  g_free (preedit_string);

  eel_editable_label_recompute (label);  
  gtk_widget_queue_resize (GTK_WIDGET (label));
}

static gboolean
eel_editable_label_retrieve_surrounding_cb (GtkIMContext *context,
					    EelEditableLabel  *label)
{
  gtk_im_context_set_surrounding (context,
				  label->text,
				  strlen (label->text) + 1,
				  label->selection_end);

  return TRUE;
}

static gboolean
eel_editable_label_delete_surrounding_cb (GtkIMContext *slave,
					  gint          offset,
					  gint          n_chars,
					  EelEditableLabel  *label)
{
  gint current_pos;

  current_pos = g_utf8_pointer_to_offset (label->text, label->text + label->selection_anchor);
  gtk_editable_delete_text (GTK_EDITABLE (label), 
			    current_pos + offset, 
			    current_pos + offset + n_chars);

  return TRUE;
}

static gboolean
eel_editable_label_focus (GtkWidget         *widget,
			  GtkDirectionType   direction)
{
  /* We never want to be in the tab chain */
  return FALSE;
}

/* Compute the X position for an offset that corresponds to the "more important
 * cursor position for that offset. We use this when trying to guess to which
 * end of the selection we should go to when the user hits the left or
 * right arrow key.
 */
static void
get_better_cursor (EelEditableLabel *label,
		   gint      index,
		   gint      *x,
		   gint      *y)
{
  GtkTextDirection keymap_direction =
    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;
  GtkTextDirection widget_direction = gtk_widget_get_direction (GTK_WIDGET (label));
  gboolean split_cursor;
  PangoRectangle strong_pos, weak_pos;
  
  g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		"gtk-split-cursor", &split_cursor,
		NULL);

  eel_editable_label_get_cursor_pos (label, &strong_pos, &weak_pos);

  if (split_cursor)
    {
      *x = strong_pos.x / PANGO_SCALE;
      *y = strong_pos.y / PANGO_SCALE;
    }
  else
    {
      if (keymap_direction == widget_direction)
	{
	  *x = strong_pos.x / PANGO_SCALE;
	  *y = strong_pos.y / PANGO_SCALE;
	}
      else
	{
	  *x = weak_pos.x / PANGO_SCALE;
	  *y = weak_pos.y / PANGO_SCALE;
	}
    }
}


static gint
eel_editable_label_move_logically (EelEditableLabel *label,
				   gint      start,
				   gint      count)
{
  gint offset = g_utf8_pointer_to_offset (label->text,
					  label->text + start);

  if (label->text)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;
      gint length;

      eel_editable_label_ensure_layout (label, FALSE);
      
      length = g_utf8_strlen (label->text, -1);

      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);

      while (count > 0 && offset < length)
	{
	  do
	    offset++;
	  while (offset < length && !log_attrs[offset].is_cursor_position);
	  
	  count--;
	}
      while (count < 0 && offset > 0)
	{
	  do
	    offset--;
	  while (offset > 0 && !log_attrs[offset].is_cursor_position);
	  
	  count++;
	}
      
      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, offset) - label->text;
}

static gint
eel_editable_label_move_visually (EelEditableLabel *label,
				  gint      start,
				  gint      count)
{
  gint index;

  index = start;
  
  while (count != 0)
    {
      int new_index, new_trailing;
      gboolean split_cursor;
      gboolean strong;

      eel_editable_label_ensure_layout (label, FALSE);

      g_object_get (gtk_widget_get_settings (GTK_WIDGET (label)),
		    "gtk-split-cursor", &split_cursor,
		    NULL);

      if (split_cursor)
	strong = TRUE;
      else
	{
	  GtkTextDirection keymap_direction =
	    (gdk_keymap_get_direction (gdk_keymap_get_default ()) == PANGO_DIRECTION_LTR) ?
	    GTK_TEXT_DIR_LTR : GTK_TEXT_DIR_RTL;

	  strong = keymap_direction == gtk_widget_get_direction (GTK_WIDGET (label));
	}
      
      if (count > 0)
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, 1, &new_index, &new_trailing);
	  count--;
	}
      else
	{
	  pango_layout_move_cursor_visually (label->layout, strong, index, 0, -1, &new_index, &new_trailing);
	  count++;
	}

      if (new_index < 0 || new_index == G_MAXINT)
	break;

      index = new_index;
      
      while (new_trailing--)
	index = g_utf8_next_char (label->text + new_index) - label->text;
    }
  
  return index;
}

static gint
eel_editable_label_move_line (EelEditableLabel *label,
			      gint      start,
			      gint      count)
{
  int n_lines, i;
  int x;
  PangoLayoutLine *line;
  int index;
  
  eel_editable_label_ensure_layout (label, FALSE);

  n_lines = pango_layout_get_line_count (label->layout);

  for (i = 0; i < n_lines; i++)
    {
      line = pango_layout_get_line (label->layout, i);
      if (start >= line->start_index &&
	  start <= line->start_index + line->length)
	{
	  pango_layout_line_index_to_x (line, start, FALSE, &x);
	  break;
	}
    }
  if (i == n_lines)
    i = n_lines - 1;
  
  i += count;
  i = CLAMP (i, 0, n_lines - 1);

  line = pango_layout_get_line (label->layout, i);
  if (pango_layout_line_x_to_index (line,
				    x,
				    &index, NULL))
    return index;
  else
    {
      if (i == n_lines - 1)
	return line->start_index + line->length;
      else
	return line->start_index + line->length - 1;
    }
}

static gint
eel_editable_label_move_forward_word (EelEditableLabel *label,
				      gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->text,
					   label->text + start);
  gint length;

  length = g_utf8_strlen (label->text, -1);
  if (new_pos < length)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      eel_editable_label_ensure_layout (label, FALSE);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);

      /* Find the next word end,
	 (remember, n_attrs is one more than the number of of chars) */
      new_pos++;
      while (new_pos < (n_attrs - 1) && !log_attrs[new_pos].is_word_end)
	new_pos++;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, new_pos) - label->text;
}


static gint
eel_editable_label_move_backward_word (EelEditableLabel *label,
				       gint      start)
{
  gint new_pos = g_utf8_pointer_to_offset (label->text,
					   label->text + start);
  
  if (new_pos > 0)
    {
      PangoLogAttr *log_attrs;
      gint n_attrs;

      eel_editable_label_ensure_layout (label, FALSE);
      
      pango_layout_get_log_attrs (label->layout, &log_attrs, &n_attrs);
      
      new_pos -= 1;

      /* Find the previous word beginning */
      while (new_pos > 0 && !log_attrs[new_pos].is_word_start)
	new_pos--;

      g_free (log_attrs);
    }

  return g_utf8_offset_to_pointer (label->text, new_pos) - label->text;
}

static void
eel_editable_label_move_cursor (EelEditableLabel    *label,
				GtkMovementStep      step,
				gint                 count,
				gboolean             extend_selection)
{
  gint new_pos;
  
  new_pos = label->selection_end;

  if (label->selection_end != label->selection_anchor &&
      !extend_selection)
    {
      /* If we have a current selection and aren't extending it, move to the
       * start/or end of the selection as appropriate
       */
      switch (step)
	{
	case GTK_MOVEMENT_DISPLAY_LINES:
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  {
	    gint end_x, end_y;
	    gint anchor_x, anchor_y;
	    gboolean end_is_left;
	    
	    get_better_cursor (label, label->selection_end, &end_x, &end_y);
	    get_better_cursor (label, label->selection_anchor, &anchor_x, &anchor_y);

	    end_is_left = (end_y < anchor_y) || (end_y == anchor_y && end_x < anchor_x);
	    
	    if (count < 0)
	      new_pos = end_is_left ? label->selection_end : label->selection_anchor;
	    else
	      new_pos = !end_is_left ? label->selection_end : label->selection_anchor;

	    break;
	  }
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	case GTK_MOVEMENT_WORDS:
	  if (count < 0)
	    new_pos = MIN (label->selection_end, label->selection_anchor);
	  else
	    new_pos = MAX (label->selection_end, label->selection_anchor);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->text);
	  break;
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }
  else
    {
      switch (step)
	{
	case GTK_MOVEMENT_LOGICAL_POSITIONS:
	  new_pos = eel_editable_label_move_logically (label, new_pos, count);
	  break;
	case GTK_MOVEMENT_VISUAL_POSITIONS:
	  new_pos = eel_editable_label_move_visually (label, new_pos, count);
	  break;
	case GTK_MOVEMENT_WORDS:
	  while (count > 0)
	    {
	      new_pos = eel_editable_label_move_forward_word (label, new_pos);
	      count--;
	    }
	  while (count < 0)
	    {
	      new_pos = eel_editable_label_move_backward_word (label, new_pos);
	      count++;
	    }
	  break;
	case GTK_MOVEMENT_DISPLAY_LINE_ENDS:
	case GTK_MOVEMENT_PARAGRAPH_ENDS:
	case GTK_MOVEMENT_BUFFER_ENDS:
	  /* FIXME: Can do better here */
	  new_pos = count < 0 ? 0 : strlen (label->text);
	  break;
	case GTK_MOVEMENT_DISPLAY_LINES:
	  new_pos = eel_editable_label_move_line (label, new_pos, count);
	  break;
	  break;
	case GTK_MOVEMENT_PARAGRAPHS:
	case GTK_MOVEMENT_PAGES:
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }

  if (extend_selection)
    eel_editable_label_select_region_index (label,
					    label->selection_anchor,
					    new_pos);
  else
    eel_editable_label_select_region_index (label, new_pos, new_pos);
}

static void
eel_editable_label_reset_im_context (EelEditableLabel  *label)
{
  if (label->need_im_reset)
    {
      label->need_im_reset = 0;
      gtk_im_context_reset (label->im_context);
    }
}


static void
eel_editable_label_delete_from_cursor (EelEditableLabel *label,
				       GtkDeleteType     type,
				       gint              count)
{
  GtkEditable *editable = GTK_EDITABLE (label);
  gint start_pos = label->selection_anchor;
  gint end_pos = label->selection_anchor;
  
  eel_editable_label_reset_im_context (label);

  if (label->selection_anchor != label->selection_end)
    {
      gtk_editable_delete_selection (editable);
      return;
    }
  
  switch (type)
    {
    case GTK_DELETE_CHARS:
      end_pos = eel_editable_label_move_logically (label, start_pos, count);
      start_pos = g_utf8_pointer_to_offset (label->text, label->text + start_pos);
      end_pos = g_utf8_pointer_to_offset (label->text, label->text + end_pos);
      gtk_editable_delete_text (GTK_EDITABLE (label), MIN (start_pos, end_pos), MAX (start_pos, end_pos));
      break;
    case GTK_DELETE_WORDS:
      if (count < 0)
	{
	  /* Move to end of current word, or if not on a word, end of previous word */
	  end_pos = eel_editable_label_move_backward_word (label, end_pos);
	  end_pos = eel_editable_label_move_forward_word (label, end_pos);
	}
      else if (count > 0)
	{
	  /* Move to beginning of current word, or if not on a word, begining of next word */
	  start_pos = eel_editable_label_move_forward_word (label, start_pos);
	  start_pos = eel_editable_label_move_backward_word (label, start_pos);
	}
	
      /* Fall through */
    case GTK_DELETE_WORD_ENDS:
      while (count < 0)
	{
	  start_pos = eel_editable_label_move_backward_word (label, start_pos);
	  count++;
	}
      while (count > 0)
	{
	  end_pos = eel_editable_label_move_forward_word (label, end_pos);
	  count--;
	}
      start_pos = g_utf8_pointer_to_offset (label->text, label->text + start_pos);
      end_pos = g_utf8_pointer_to_offset (label->text, label->text + end_pos);
 
      gtk_editable_delete_text (GTK_EDITABLE (label), start_pos, end_pos);
      break;
    case GTK_DELETE_DISPLAY_LINE_ENDS:
    case GTK_DELETE_PARAGRAPH_ENDS:
      end_pos = g_utf8_pointer_to_offset (label->text, label->text + label->selection_anchor);
      if (count < 0)
	gtk_editable_delete_text (GTK_EDITABLE (label), 0, end_pos);
      else
	gtk_editable_delete_text (GTK_EDITABLE (label), end_pos, -1);
      break;
    case GTK_DELETE_DISPLAY_LINES:
    case GTK_DELETE_PARAGRAPHS:
      gtk_editable_delete_text (GTK_EDITABLE (label), 0, -1);  
      break;
    case GTK_DELETE_WHITESPACE:
      /* TODO eel_editable_label_delete_whitespace (label); */
      break;
    }
  
  eel_editable_label_pend_cursor_blink (label);
}


static void
eel_editable_label_copy_clipboard (EelEditableLabel *label)
{
  if (label->text)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->selection_anchor,
                   label->selection_end);
      end = MAX (label->selection_anchor,
                 label->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      if (start != end)
	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				label->text + start, end - start);
    }
}

static void
eel_editable_label_cut_clipboard (EelEditableLabel *label)
{
  if (label->text)
    {
      gint start, end;
      gint len;
      
      start = MIN (label->selection_anchor,
                   label->selection_end);
      end = MAX (label->selection_anchor,
                 label->selection_end);

      len = strlen (label->text);

      if (end > len)
        end = len;

      if (start > len)
        start = len;

      if (start != end)
	{
	  gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
				  label->text + start, end - start);
          start = g_utf8_pointer_to_offset (label->text, label->text + start);
          end = g_utf8_pointer_to_offset (label->text, label->text + end);
	  gtk_editable_delete_text (GTK_EDITABLE (label), start, end);
	}
    }
}

static void
paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer      data)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (data);
  GtkEditable *editable = GTK_EDITABLE (label);
  gint tmp_pos;
      
  if (text)
    {
      if (label->selection_end != label->selection_anchor)
	gtk_editable_delete_selection (editable);

      tmp_pos = g_utf8_pointer_to_offset (label->text,
                                      label->text + label->selection_anchor);
      gtk_editable_insert_text (GTK_EDITABLE (label), text, strlen (text), &tmp_pos);
      tmp_pos = g_utf8_offset_to_pointer (label->text, tmp_pos) - label->text;
      eel_editable_label_select_region_index (label, tmp_pos, tmp_pos);
    }

  g_object_unref (G_OBJECT (label));
}

static void
eel_editable_label_paste (EelEditableLabel *label,
			  GdkAtom   selection)
{
  g_object_ref (G_OBJECT (label));
  gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (label), selection),
			      paste_received, label);
}

static void
eel_editable_label_paste_clipboard (EelEditableLabel *label)
{
  eel_editable_label_paste (label, GDK_NONE);
}

static void
eel_editable_label_select_all (EelEditableLabel *label)
{
  eel_editable_label_select_region_index (label, 0, strlen (label->text));
}

/* Quick hack of a popup menu
 */
static void
activate_cb (GtkWidget *menuitem,
	     EelEditableLabel  *label)
{
  const gchar *signal = g_object_get_data (G_OBJECT (menuitem), "gtk-signal");
  g_signal_emit_by_name (label, signal);
}

static void
append_action_signal (EelEditableLabel     *label,
		      GtkWidget    *menu,
		      const gchar  *stock_id,
		      const gchar  *signal,
                      gboolean      sensitive)
{
  GtkWidget *menuitem = gtk_image_menu_item_new_from_stock (stock_id, NULL);

  g_object_set_data (G_OBJECT (menuitem), "gtk-signal", (char *)signal);
  g_signal_connect (menuitem, "activate",
		    G_CALLBACK (activate_cb), label);

  gtk_widget_set_sensitive (menuitem, sensitive);
  
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);
}

static void
popup_menu_detach (GtkWidget *attach_widget,
		   GtkMenu   *menu)
{
  EelEditableLabel *label;
  label = EEL_EDITABLE_LABEL (attach_widget);

  label->popup_menu = NULL;
}

static void
popup_position_func (GtkMenu   *menu,
                     gint      *x,
                     gint      *y,
                     gboolean  *push_in,
                     gpointer	user_data)
{
  EelEditableLabel *label;
  GtkWidget *widget;
  GtkRequisition req;
  GtkAllocation allocation;
  
  label = EEL_EDITABLE_LABEL (user_data);  
  widget = GTK_WIDGET (label);

  g_assert (gtk_widget_get_realized (widget));

  gdk_window_get_origin (gtk_widget_get_window (widget), x, y);

  /*gtk_widget_size_request (label->popup_menu, &req);*/
  gtk_widget_get_requisition (widget, &req);
  gtk_widget_get_allocation (widget, &allocation);
  
  *x += allocation.width / 2;
  *y += allocation.height;

  *x = CLAMP (*x, 0, MAX (0, gdk_screen_width () - req.width));
  *y = CLAMP (*y, 0, MAX (0, gdk_screen_height () - req.height));
}

static void
eel_editable_label_toggle_overwrite (EelEditableLabel *label)
{
  label->overwrite_mode = !label->overwrite_mode;
  gtk_widget_queue_draw (GTK_WIDGET (label));
}

typedef struct
{
  EelEditableLabel *label;
  gint button;
  guint time;
} PopupInfo;

static void
popup_targets_received (GtkClipboard     *clipboard,
			GtkSelectionData *data,
			gpointer          user_data)
{
  GtkWidget *menuitem, *submenu;
  gboolean have_selection;
  gboolean clipboard_contains_text;
  PopupInfo *info;
  EelEditableLabel *label;

  info = user_data;
  label = info->label;

  if (gtk_widget_get_realized (GTK_WIDGET (label)))
    {
      if (label->popup_menu)
	gtk_widget_destroy (label->popup_menu);
  
      label->popup_menu = gtk_menu_new ();

      gtk_menu_attach_to_widget (GTK_MENU (label->popup_menu),
				 GTK_WIDGET (label),
				 popup_menu_detach);

      have_selection =
	label->selection_anchor != label->selection_end;
  
      clipboard_contains_text = gtk_selection_data_targets_include_text (data);

      append_action_signal (label, label->popup_menu, GTK_STOCK_CUT, "cut_clipboard",
			    have_selection);
      append_action_signal (label, label->popup_menu, GTK_STOCK_COPY, "copy_clipboard",
			    have_selection);
      append_action_signal (label, label->popup_menu, GTK_STOCK_PASTE, "paste_clipboard",
			    clipboard_contains_text);
  
      menuitem = gtk_menu_item_new_with_label (_("Select All"));
      g_signal_connect_object (menuitem, "activate",
			       G_CALLBACK (eel_editable_label_select_all), label,
			       G_CONNECT_SWAPPED);
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (label->popup_menu), menuitem);

      menuitem = gtk_separator_menu_item_new ();
      gtk_widget_show (menuitem);
      gtk_menu_shell_append (GTK_MENU_SHELL (label->popup_menu), menuitem);
      
      menuitem = gtk_menu_item_new_with_label (_("Input Methods"));
      gtk_widget_show (menuitem);
      submenu = gtk_menu_new ();
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
      
      gtk_menu_shell_append (GTK_MENU_SHELL (label->popup_menu), menuitem);
  
      gtk_im_multicontext_append_menuitems (GTK_IM_MULTICONTEXT (label->im_context),
					    GTK_MENU_SHELL (submenu));

      g_signal_emit (label,
		     signals[POPULATE_POPUP], 0,
		     label->popup_menu);

      if (info->button)
	gtk_menu_popup (GTK_MENU (label->popup_menu), NULL, NULL,
			NULL, NULL,
			info->button, info->time);
      else
	{
	  gtk_menu_popup (GTK_MENU (label->popup_menu), NULL, NULL,
			  popup_position_func, label,
			  info->button, info->time);
	  gtk_menu_shell_select_first (GTK_MENU_SHELL (label->popup_menu), FALSE);
	}
    }

  g_object_unref (label);
  g_free (info);
}

static void
eel_editable_label_do_popup (EelEditableLabel *label,
			     GdkEventButton   *event)
{
  PopupInfo *info = g_new (PopupInfo, 1);

  /* In order to know what entries we should make sensitive, we
   * ask for the current targets of the clipboard, and when
   * we get them, then we actually pop up the menu.
   */
  info->label = g_object_ref (label);
  
  if (event)
    {
      info->button = event->button;
      info->time = event->time;
    }
  else
    {
      info->button = 0;
      info->time = gtk_get_current_event_time ();
    }

  gtk_clipboard_request_contents (gtk_widget_get_clipboard (GTK_WIDGET (label), GDK_SELECTION_CLIPBOARD),
				  gdk_atom_intern ("TARGETS", FALSE),
				  popup_targets_received,
				  info);
}

/************ Editable implementation ****************/

static void
editable_insert_text_emit  (GtkEditable *editable,
			    const gchar *new_text,
			    gint         new_text_length,
			    gint        *position)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  gchar buf[64];
  gchar *text;
  int text_length;

  text_length = g_utf8_strlen (label->text, -1);

  if (*position < 0 || *position > text_length)
    *position = text_length;
  
  g_object_ref (G_OBJECT (editable));
  
  if (new_text_length <= 63)
    text = buf;
  else
    text = g_new (gchar, new_text_length + 1);

  text[new_text_length] = '\0';
  strncpy (text, new_text, new_text_length);
  
  g_signal_emit_by_name (editable, "insert_text", text, new_text_length, position);

  if (new_text_length > 63)
    g_free (text);

  g_object_unref (G_OBJECT (editable));
}

static void
editable_delete_text_emit (GtkEditable *editable,
			   gint         start_pos,
			   gint         end_pos)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  int text_length;

  text_length = g_utf8_strlen (label->text, -1);

  if (end_pos < 0 || end_pos > text_length)
    end_pos = text_length;
  if (start_pos < 0)
    start_pos = 0;
  if (start_pos > end_pos)
    start_pos = end_pos;
  
  g_object_ref (G_OBJECT (editable));

  g_signal_emit_by_name (editable, "delete_text", start_pos, end_pos);

  g_object_unref (G_OBJECT (editable));
}

static void
editable_insert_text (GtkEditable *editable,
		      const gchar *new_text,
		      gint         new_text_length,
		      gint        *position)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  gint index;

  if (new_text_length < 0)
    new_text_length = strlen (new_text);

  index = g_utf8_offset_to_pointer (label->text, *position) - label->text;

  eel_editable_label_insert_text (label,
				  new_text,
				  new_text_length,
				  &index);
  
  *position = g_utf8_pointer_to_offset (label->text, label->text + index);
}

static void
editable_delete_text (GtkEditable *editable,
		      gint         start_pos,
		      gint         end_pos)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  int text_length;
  gint start_index, end_index;

  text_length = g_utf8_strlen (label->text, -1);

  if (end_pos < 0 || end_pos > text_length)
    end_pos = text_length;
  if (start_pos < 0)
    start_pos = 0;
  if (start_pos > end_pos)
    start_pos = end_pos;

  start_index = g_utf8_offset_to_pointer (label->text, start_pos) - label->text;
  end_index = g_utf8_offset_to_pointer (label->text, end_pos) - label->text;
  
  eel_editable_label_delete_text (label, start_index, end_index);
}

static gchar *    
editable_get_chars (GtkEditable   *editable,
		    gint           start_pos,
		    gint           end_pos)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  int text_length;
  gint start_index, end_index;
  
  text_length = g_utf8_strlen (label->text, -1);

  if (end_pos < 0 || end_pos > text_length)
    end_pos = text_length;
  if (start_pos < 0)
    start_pos = 0;
  if (start_pos > end_pos)
    start_pos = end_pos;

  start_index = g_utf8_offset_to_pointer (label->text, start_pos) - label->text;
  end_index = g_utf8_offset_to_pointer (label->text, end_pos) - label->text;

  return g_strndup (label->text + start_index, end_index - start_index);
}

static void
editable_set_selection_bounds (GtkEditable *editable,
			       gint         start,
			       gint         end)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  int text_length;
  gint start_index, end_index;
  
  text_length = g_utf8_strlen (label->text, -1);

  if (end < 0 || end > text_length)
    end = text_length;
  if (start < 0)
    start = text_length;
  if (start > text_length)
    start = text_length;
  
  eel_editable_label_reset_im_context (label);

  start_index = g_utf8_offset_to_pointer (label->text, start) - label->text;
  end_index = g_utf8_offset_to_pointer (label->text, end) - label->text;

  eel_editable_label_select_region_index (label, start_index, end_index);
}

static gboolean
editable_get_selection_bounds (GtkEditable *editable,
			       gint        *start,
			       gint        *end)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);

  *start = g_utf8_pointer_to_offset (label->text, label->text + label->selection_anchor);
  *end = g_utf8_pointer_to_offset (label->text, label->text + label->selection_end);

  return (label->selection_anchor != label->selection_end);
}

static void
editable_real_set_position (GtkEditable *editable,
			    gint         position)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  int text_length;
  int index;
  
  text_length = g_utf8_strlen (label->text, -1);
  
  if (position < 0 || position > text_length)
    position = text_length;

  index = g_utf8_offset_to_pointer (label->text, position) - label->text;
  
  if (index != label->selection_anchor ||
      index != label->selection_end)
    {
      eel_editable_label_select_region_index (label, index, index);
    }
}

static gint
editable_get_position (GtkEditable *editable)
{
  EelEditableLabel *label = EEL_EDITABLE_LABEL (editable);
  
  return g_utf8_pointer_to_offset (label->text, label->text + label->selection_anchor);
}


static AtkObjectClass *a11y_parent_class = NULL;

static const char* eel_editable_label_accessible_data = "eel-editable-label-accessible-data";

/************ Accessible implementation ****************/

typedef struct {
	GailTextUtil *textutil;
        gint         selection_anchor;
        gint         selection_end;
        gchar        *signal_name;
        gint         position;
        gint         length;
} EelEditableLabelAccessiblePrivate;

typedef struct
{
  EelEditableLabel* label;
  gint position;
} EelEditableLabelAccessiblePaste;


static gchar*
eel_editable_label_accessible_get_text (AtkText *text,
					gint    start_pos,
					gint    end_pos)
{
  GtkWidget *widget;
  EelEditableLabelAccessiblePrivate *priv;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
  /* State is defunct */
    return NULL;

  priv = g_object_get_data (G_OBJECT (text), eel_editable_label_accessible_data);
  return gail_text_util_get_substring (priv->textutil, start_pos, end_pos);
}

static gunichar 
eel_editable_label_accessible_get_character_at_offset (AtkText *text,
						       gint     offset)
{
  GtkWidget *widget;
  EelEditableLabelAccessiblePrivate *priv;
  gchar *string;
  gchar *index;
  gunichar unichar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return '\0';

  priv = g_object_get_data (G_OBJECT (text), eel_editable_label_accessible_data);
  string = gail_text_util_get_substring (priv->textutil, 0, -1);
  if (offset >= g_utf8_strlen (string, -1))
    {
      unichar = '\0';
    }
  else
    {
      index = g_utf8_offset_to_pointer (string, offset);

      unichar = g_utf8_get_char(index);
    }

  g_free(string);
  return unichar;
}

static gchar*
eel_editable_label_accessible_get_text_before_offset (AtkText         *text,
						      gint            offset,
						      AtkTextBoundary boundary_type,
						      gint            *start_offset,
						      gint            *end_offset)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  EelEditableLabelAccessiblePrivate *priv;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
  /* State is defunct */
    return NULL;

  label = EEL_EDITABLE_LABEL (widget);
  priv = g_object_get_data (G_OBJECT (text), eel_editable_label_accessible_data);

  return gail_text_util_get_text (priv->textutil,
                                  eel_editable_label_get_layout (label),
                                  GAIL_BEFORE_OFFSET, 
                                  boundary_type, offset,
                                  start_offset, end_offset);
}

static gchar*
eel_editable_label_accessible_get_text_at_offset (AtkText          *text,
						  gint             offset,
						  AtkTextBoundary  boundary_type,
						  gint             *start_offset,
    						  gint             *end_offset)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  EelEditableLabelAccessiblePrivate *priv;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;


  label = EEL_EDITABLE_LABEL (widget);
  priv = g_object_get_data (G_OBJECT (text), eel_editable_label_accessible_data);
  return gail_text_util_get_text (priv->textutil,
                                  eel_editable_label_get_layout (label),
                                  GAIL_AT_OFFSET, 
                                  boundary_type, offset,
                                  start_offset, end_offset);
}

static gchar*
eel_editable_label_accessible_get_text_after_offset  (AtkText          *text,
						      gint             offset,
						      AtkTextBoundary  boundary_type,
						      gint             *start_offset,
						      gint             *end_offset)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  EelEditableLabelAccessiblePrivate *priv;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = EEL_EDITABLE_LABEL (widget);
  priv = g_object_get_data (G_OBJECT (text), eel_editable_label_accessible_data);
  return gail_text_util_get_text (priv->textutil,
                                  eel_editable_label_get_layout (label),
                                  GAIL_AFTER_OFFSET, 
                                  boundary_type, offset,
                                  start_offset, end_offset);
}

static gint
eel_editable_label_accessible_get_caret_offset (AtkText *text)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return 0;

  return gtk_editable_get_position (GTK_EDITABLE (widget));
}

static gboolean
eel_editable_label_accessible_set_caret_offset (AtkText *text, gint offset)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  gtk_editable_set_position (GTK_EDITABLE (widget), offset);
  return TRUE;
}

static gint
eel_editable_label_accessible_get_character_count (AtkText *text)
{
  GtkWidget *widget;
  EelEditableLabel *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return 0;

  label = EEL_EDITABLE_LABEL (widget);
  return g_utf8_strlen (eel_editable_label_get_text (label), -1);
}

static gint
eel_editable_label_accessible_get_n_selections (AtkText *text)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return -1;

  label = EEL_EDITABLE_LABEL (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (label), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    return 1;
  else
    return 0;
}

static gchar*
eel_editable_label_accessible_get_selection (AtkText *text,
					    gint    selection_num,
					    gint    *start_pos,
					    gint    *end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

 /* Only let the user get the selection if one is set, and if the
  * selection_num is 0.
  */
  if (selection_num != 0)
     return NULL;

  label = EEL_EDITABLE_LABEL (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (label), start_pos, end_pos);

  if (*start_pos != *end_pos)
     return gtk_editable_get_chars (GTK_EDITABLE (label), *start_pos, *end_pos);
  else
     return NULL;
}

static gboolean
eel_editable_label_accessible_add_selection (AtkText *text,
					     gint    start_pos,
					     gint    end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  label = EEL_EDITABLE_LABEL (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (label), &select_start, 
                                     &select_end);

 /* If there is already a selection, then don't allow another to be added,
  * since EelEditableLabel only supports one selected region.
  */
  if (select_start == select_end)
    {
       gtk_editable_select_region (GTK_EDITABLE (label), start_pos, end_pos);
       return TRUE;
    }
  else
   return FALSE;
}

static gboolean
eel_editable_label_accessible_remove_selection (AtkText *text,
						gint    selection_num)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  gint select_start, select_end, caret_pos;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  if (selection_num != 0)
     return FALSE;

  label = EEL_EDITABLE_LABEL (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (label), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    {
     /* Setting the start & end of the selected region to the caret position
      * turns off the selection.
      */
      caret_pos = gtk_editable_get_position (GTK_EDITABLE (label));
      gtk_editable_select_region (GTK_EDITABLE (label), caret_pos, caret_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
eel_editable_label_accessible_set_selection (AtkText *text,
					     gint    selection_num,
					     gint    start_pos,
					     gint    end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  gint select_start, select_end;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

 /* Only let the user move the selection if one is set, and if the
  * selection_num is 0
  */
  if (selection_num != 0)
     return FALSE;

  label = EEL_EDITABLE_LABEL (widget);
  gtk_editable_get_selection_bounds (GTK_EDITABLE (label), &select_start, 
                                     &select_end);

  if (select_start != select_end)
    {
      gtk_editable_select_region (GTK_EDITABLE (label), start_pos, end_pos);
      return TRUE;
    }
  else
    return FALSE;
}

static AtkAttributeSet*
eel_editable_label_accessible_get_run_attributes (AtkText *text,
						  gint    offset,
						  gint    *start_offset,
						  gint    *end_offset)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  AtkAttributeSet *at_set = NULL;
  GtkTextDirection dir;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = EEL_EDITABLE_LABEL (widget);
 
  dir = gtk_widget_get_direction (widget);
  if (dir == GTK_TEXT_DIR_RTL)
    {
      at_set = gail_misc_add_attribute (at_set,
                                        ATK_TEXT_ATTR_DIRECTION,
       g_strdup (atk_text_attribute_get_value (ATK_TEXT_ATTR_DIRECTION, dir)));
    }

  at_set = gail_misc_layout_get_run_attributes (at_set,
                                                eel_editable_label_get_layout (label),
                                                label->text,
                                                offset,
                                                start_offset,
                                                end_offset);
  return at_set;
}

static AtkAttributeSet*
eel_editable_label_accessible_get_default_attributes (AtkText *text)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  AtkAttributeSet *at_set = NULL;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  label = EEL_EDITABLE_LABEL (widget);

  at_set = gail_misc_get_default_attributes (at_set,
                                             eel_editable_label_get_layout (label),
                                             widget);
  return at_set;
}

static void
eel_editable_label_accessible_get_character_extents (AtkText      *text,
						     gint         offset,
						     gint         *x,
						     gint         *y,
						     gint         *width,
						     gint         *height,
						     AtkCoordType coords)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  PangoRectangle char_rect;
  gint index, cursor_index, x_layout, y_layout;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);
  eel_editable_label_get_layout_offsets (label, &x_layout, &y_layout);
  index = g_utf8_offset_to_pointer (label->text, offset) - label->text;
  cursor_index = label->selection_anchor;
  if (index > cursor_index)
    index += label->preedit_length;
  pango_layout_index_to_pos (eel_editable_label_get_layout(label), index, &char_rect);
 
  gail_misc_get_extents_from_pango_rectangle (widget, &char_rect, 
                        x_layout, y_layout, x, y, width, height, coords);
} 

static gint 
eel_editable_label_accessible_get_offset_at_point (AtkText      *text,
						   gint         x,
						   gint         y,
						   AtkCoordType coords)
{ 
  GtkWidget *widget;
  EelEditableLabel *label;
  gint index, cursor_index, x_layout, y_layout;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return -1;

  label = EEL_EDITABLE_LABEL (widget);
  
  eel_editable_label_get_layout_offsets (label, &x_layout, &y_layout);
  
  index = gail_misc_get_index_at_point_in_layout (widget, 
               eel_editable_label_get_layout(label), x_layout, y_layout, x, y, coords);
  if (index == -1)
    {
      if (coords == ATK_XY_SCREEN || coords == ATK_XY_WINDOW)
        return g_utf8_strlen (label->text, -1);

      return index;  
    }
  else
    {
      cursor_index = label->selection_anchor;
      if (index >= cursor_index && label->preedit_length)
        {
          if (index >= cursor_index + label->preedit_length)
            index -= label->preedit_length;
          else
            index = cursor_index;
        }
      return g_utf8_pointer_to_offset (label->text, label->text + index);
    }
}

static void
atk_text_interface_init (AtkTextIface *iface)
{
  g_assert (iface != NULL);

  iface->get_text = eel_editable_label_accessible_get_text;
  iface->get_character_at_offset = eel_editable_label_accessible_get_character_at_offset;
  iface->get_text_before_offset = eel_editable_label_accessible_get_text_before_offset;
  iface->get_text_at_offset = eel_editable_label_accessible_get_text_at_offset;
  iface->get_text_after_offset = eel_editable_label_accessible_get_text_after_offset;
  iface->get_caret_offset = eel_editable_label_accessible_get_caret_offset;
  iface->set_caret_offset = eel_editable_label_accessible_set_caret_offset;
  iface->get_character_count = eel_editable_label_accessible_get_character_count;
  iface->get_n_selections = eel_editable_label_accessible_get_n_selections;
  iface->get_selection = eel_editable_label_accessible_get_selection;
  iface->add_selection = eel_editable_label_accessible_add_selection;
  iface->remove_selection = eel_editable_label_accessible_remove_selection;
  iface->set_selection = eel_editable_label_accessible_set_selection;
  iface->get_run_attributes = eel_editable_label_accessible_get_run_attributes;
  iface->get_default_attributes = eel_editable_label_accessible_get_default_attributes;
  iface->get_character_extents = eel_editable_label_accessible_get_character_extents;
  iface->get_offset_at_point = eel_editable_label_accessible_get_offset_at_point;
}

static void
eel_editable_label_accessible_set_text_contents (AtkEditableText *text,
						 const gchar     *string)
{
  GtkWidget *widget;
  EelEditableLabel *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);

  eel_editable_label_set_text (label, string);
}

static void
eel_editable_label_accessible_insert_text (AtkEditableText *text,
					   const gchar     *string,
					   gint            length,
					   gint            *position)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);
  editable = GTK_EDITABLE (label);

  gtk_editable_insert_text (editable, string, length, position);
}

static void
eel_editable_label_accessible_copy_text   (AtkEditableText *text,
					   gint            start_pos,
					   gint            end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  GtkEditable *editable;
  gchar *str;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);
  editable = GTK_EDITABLE (label);
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), str, -1);
}

static void
eel_editable_label_accessible_cut_text (AtkEditableText *text,
					gint            start_pos,
					gint            end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  GtkEditable *editable;
  gchar *str;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);
  editable = GTK_EDITABLE (label);
  str = gtk_editable_get_chars (editable, start_pos, end_pos);
  gtk_clipboard_set_text (gtk_clipboard_get (GDK_NONE), str, -1);
  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
eel_editable_label_accessible_delete_text (AtkEditableText *text,
					   gint            start_pos,
					   gint            end_pos)
{
  GtkWidget *widget;
  EelEditableLabel *label;
  GtkEditable *editable;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  label = EEL_EDITABLE_LABEL (widget);
  editable = GTK_EDITABLE (label);

  gtk_editable_delete_text (editable, start_pos, end_pos);
}

static void
eel_editable_label_accessible_paste_received (GtkClipboard *clipboard,
					      const gchar  *text,
					      gpointer     data)
{
  EelEditableLabelAccessiblePaste* paste_struct = (EelEditableLabelAccessiblePaste *)data;

  if (text)
    gtk_editable_insert_text (GTK_EDITABLE (paste_struct->label), text, -1,
       &(paste_struct->position));

  g_object_unref (paste_struct->label);
}

static void
eel_editable_label_accessible_paste_text (AtkEditableText *text,
					  gint            position)
{
  GtkWidget *widget;
  GtkEditable *editable;
  EelEditableLabelAccessiblePaste paste_struct;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (text));
  if (widget == NULL)
    /* State is defunct */
    return;

  editable = GTK_EDITABLE (widget);
  if (!gtk_editable_get_editable (editable))
    return;
  paste_struct.label = EEL_EDITABLE_LABEL (widget);
  paste_struct.position = position;

  g_object_ref (paste_struct.label);
  gtk_clipboard_request_text (gtk_clipboard_get (GDK_NONE),
    eel_editable_label_accessible_paste_received, &paste_struct);
}

static void
atk_editable_text_interface_init (AtkEditableTextIface *iface)
{
  g_assert (iface != NULL);

  iface->set_text_contents = eel_editable_label_accessible_set_text_contents;
  iface->insert_text = eel_editable_label_accessible_insert_text;
  iface->copy_text = eel_editable_label_accessible_copy_text;
  iface->cut_text = eel_editable_label_accessible_cut_text;
  iface->delete_text = eel_editable_label_accessible_delete_text;
  iface->paste_text = eel_editable_label_accessible_paste_text;
}

static void
eel_editable_label_accessible_notify_insert (AtkObject *accessible)
{
  EelEditableLabelAccessiblePrivate *priv;

  priv = g_object_get_data (G_OBJECT (accessible), eel_editable_label_accessible_data);
  if (priv->signal_name)
    {
      g_signal_emit_by_name (accessible, 
                             priv->signal_name,
                             priv->position,
                             priv->length);
      priv->signal_name = NULL;
    }
}

static gboolean
eel_editable_label_accessible_idle_notify_insert (gpointer data)
{
  eel_editable_label_accessible_notify_insert (data);
  return FALSE;
}

/* Note arg1 returns the character at the start of the insert.
 * arg2 returns the number of characters inserted.
 */
static void 
eel_editable_label_accessible_insert_text_cb (EelEditableLabel *label, 
                                              gchar            *arg1, 
                                              gint             arg2,
                                              gpointer         arg3)
{
  AtkObject *accessible;
  EelEditableLabelAccessiblePrivate *priv;
  gint *position = (gint *) arg3;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (label));
  priv = g_object_get_data (G_OBJECT (accessible), eel_editable_label_accessible_data);
  if (!priv->signal_name)
    {
      priv->signal_name = "text_changed::insert";
      priv->position = *position;
      priv->length = arg2;
    }
  /*
   * The signal will be emitted when the cursor position is updated.
   * or in an idle handler if it not updated.
   */
   g_idle_add (eel_editable_label_accessible_idle_notify_insert, accessible);
}

/* Note arg1 returns the start of the delete range, arg2 returns the
 * end of the delete range if multiple characters are deleted.	
 */
static void 
eel_editable_label_accessible_delete_text_cb (EelEditableLabel *label,
					      gint             arg1, 
					      gint             arg2)
{
  AtkObject *accessible;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (label));

  /*
   * Zero length text deleted so ignore
   */
  if (arg2 - arg1 == 0)
    return;

  g_signal_emit_by_name (accessible, "text_changed::delete", arg1, arg2 - arg1);
}

static void
eel_editable_label_accessible_changed_cb (EelEditableLabel *label)
{
  AtkObject *accessible;
  EelEditableLabelAccessiblePrivate *priv;

  accessible = gtk_widget_get_accessible (GTK_WIDGET (label));
  priv = g_object_get_data (G_OBJECT (accessible), eel_editable_label_accessible_data);
  gail_text_util_text_setup (priv->textutil, eel_editable_label_get_text (label));
}

static gboolean 
check_for_selection_change (AtkObject   *accessible,
                            GtkWidget   *widget)
{
  EelEditableLabelAccessiblePrivate *priv;
  EelEditableLabel *label;
  gboolean ret_val = FALSE;

  priv = g_object_get_data (G_OBJECT (accessible), eel_editable_label_accessible_data);
  label = EEL_EDITABLE_LABEL (widget);
 
  if (label->selection_anchor != label->selection_end)
    {
      if (label->selection_anchor != priv->selection_anchor ||
          label->selection_end != priv->selection_end)
        /*
         * This check is here as this function can be called
         * for notification of selection_end and selection_anchor.
         * The values of selection_anchor and selection_end may be the same 
         * for both notifications and we only want to generate one
         * text_selection_changed signal.
         */
        ret_val = TRUE;
    }
  else 
    {
      /* We had a selection */
      ret_val = (priv->selection_anchor != priv->selection_end);
    }
  priv->selection_anchor = label->selection_anchor;
  priv->selection_end = label->selection_end;

  return ret_val;
}

static void
eel_editable_label_accessible_notify_gtk (GObject    *obj,
                                          GParamSpec *pspec)
{
  GtkWidget *widget;
  AtkObject *accessible;
  EelEditableLabel *label;

  widget = GTK_WIDGET (obj);
  label = EEL_EDITABLE_LABEL (widget);
  accessible = gtk_widget_get_accessible (widget);

  if (strcmp (pspec->name, "cursor-position") == 0)
    {
      eel_editable_label_accessible_notify_insert (accessible);
      if (check_for_selection_change (accessible, widget))
        g_signal_emit_by_name (accessible, "text_selection_changed");
      /*
       * The label cursor position has moved so generate the signal.
       */
      g_signal_emit_by_name (accessible, "text_caret_moved",
                             g_utf8_pointer_to_offset (label->text,
					 label->text + label->selection_anchor));
    }
  else if (strcmp (pspec->name, "selection-bound") == 0)
    {
      eel_editable_label_accessible_notify_insert (accessible);

      if (check_for_selection_change (accessible, widget))
        g_signal_emit_by_name (accessible, "text_selection_changed");
    }
}

static void
eel_editable_label_accessible_initialize (AtkObject *accessible,
                                          gpointer   widget)
{
  EelEditableLabelAccessiblePrivate *priv;
  EelEditableLabel *label;

  a11y_parent_class->initialize (accessible, widget);

  label = EEL_EDITABLE_LABEL (widget);
  priv = g_new0 (EelEditableLabelAccessiblePrivate, 1);
  priv->textutil = gail_text_util_new ();
  gail_text_util_text_setup (priv->textutil, eel_editable_label_get_text (EEL_EDITABLE_LABEL (widget)));
  priv->selection_anchor = label->selection_anchor;
  priv->selection_end = label->selection_end;
  g_object_set_data (G_OBJECT (accessible), eel_editable_label_accessible_data, priv);
  g_signal_connect (widget, "insert-text",
        G_CALLBACK (eel_editable_label_accessible_insert_text_cb), NULL);
  g_signal_connect (widget, "delete-text",
        G_CALLBACK (eel_editable_label_accessible_delete_text_cb), NULL);
  g_signal_connect (widget, "changed",
        G_CALLBACK (eel_editable_label_accessible_changed_cb), NULL);

  g_signal_connect (widget,
                    "notify",
                    G_CALLBACK (eel_editable_label_accessible_notify_gtk),
                    NULL);
  atk_object_set_role (accessible, ATK_ROLE_TEXT);
}

static const gchar*
eel_editable_label_accessible_get_name (AtkObject *accessible)
{
  if (accessible->name != NULL)
    return accessible->name;
  else
    {
      GtkWidget *widget;

      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
      if (widget == NULL)
      /* State is defunct */
        return NULL;

      g_assert (EEL_IS_EDITABLE_LABEL (widget));
      return eel_editable_label_get_text (EEL_EDITABLE_LABEL (widget));
    }
}

static AtkStateSet*
eel_editable_label_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = a11y_parent_class->ref_state_set (accessible);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
 
  if (widget == NULL)
    return state_set;

  atk_state_set_add_state (state_set, ATK_STATE_EDITABLE);
  atk_state_set_add_state (state_set, ATK_STATE_MULTI_LINE);
  return state_set;
}

static void 
eel_editable_label_accessible_finalize (GObject *object)
{
  EelEditableLabelAccessiblePrivate *priv;

  priv = g_object_get_data (object, eel_editable_label_accessible_data);
  g_object_unref (priv->textutil);
  g_free (priv);
  G_OBJECT_CLASS (a11y_parent_class)->finalize (object);
}

static void
eel_editable_label_accessible_class_init (AtkObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  a11y_parent_class = g_type_class_peek_parent (klass);

  klass->initialize = eel_editable_label_accessible_initialize;
  klass->get_name = eel_editable_label_accessible_get_name;
  klass->ref_state_set = eel_editable_label_accessible_ref_state_set;
  gobject_class->finalize = eel_editable_label_accessible_finalize;
}

static AtkObject *
eel_editable_label_get_accessible (GtkWidget *widget)
{
  static GType type = 0;
  AtkObject *accessible;

  if ((accessible = eel_accessibility_get_atk_object (widget)))
    return accessible;

  if (!type)
    {
      const GInterfaceInfo atk_editable_text_info = 
      {
        (GInterfaceInitFunc) atk_editable_text_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };
      const GInterfaceInfo atk_text_info =
      {
        (GInterfaceInitFunc) atk_text_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = eel_accessibility_create_accessible_gtype
        ("EelEditableLabelAccessible",
         widget,
         (GClassInitFunc) eel_editable_label_accessible_class_init);

      if (!type)
        return NULL;

      g_type_add_interface_static (type, ATK_TYPE_EDITABLE_TEXT, &atk_editable_text_info);
      g_type_add_interface_static (type, ATK_TYPE_TEXT, &atk_text_info);
    }

  accessible = g_object_new (type, "widget", widget, NULL);

  return eel_accessibility_set_atk_object_return (widget, accessible);
}
