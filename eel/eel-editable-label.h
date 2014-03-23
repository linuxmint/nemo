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

#ifndef __EEL_EDITABLE_LABEL_H__
#define __EEL_EDITABLE_LABEL_H__


#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EEL_TYPE_EDITABLE_LABEL eel_editable_label_get_type()
#define EEL_EDITABLE_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_EDITABLE_LABEL, EelEditableLabel))
#define EEL_EDITABLE_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_EDITABLE_LABEL, EelEditableLabelClass))
#define EEL_IS_EDITABLE_LABEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_EDITABLE_LABEL))
#define EEL_IS_EDITABLE_LABEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_EDITABLE_LABEL))
#define EEL_EDITABLE_LABEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_EDITABLE_LABEL, EelEditableLabelClass))

typedef struct _EelEditableLabel       EelEditableLabel;
typedef struct _EelEditableLabelClass  EelEditableLabelClass;

typedef struct _EelEditableLabelSelectionInfo EelEditableLabelSelectionInfo;

struct _EelEditableLabel
{
  GtkMisc misc;

  /*< private >*/
  guint   jtype : 2;
  guint   wrap : 1;
  guint   overwrite_mode : 1;
  guint   draw_outline : 1;
  PangoWrapMode  wrap_mode;

  GdkWindow *text_area;
  
  gchar  *text;
  int text_size; /* allocated size, in bytes */
  int n_bytes;	 /* length in use (not including terminating zero), in bytes */

  PangoLayout *layout;
  guint        layout_includes_preedit : 1;

  int selection_anchor; /* cursor pos, byte index */
  int selection_end; /* byte index */
  
  GtkWidget *popup_menu;

  GtkIMContext *im_context;
  gboolean need_im_reset;
  int preedit_length;	/* length of preedit string, in bytes */
  int preedit_cursor;	/* offset of cursor within preedit string, in chars */

  PangoFontDescription *font_desc;
};

struct _EelEditableLabelClass
{
  GtkMiscClass parent_class;

  void (* move_cursor)        (EelEditableLabel  *label,
			       GtkMovementStep    step,
			       gint               count,
			       gboolean           extend_selection);
  void (* insert_at_cursor)   (EelEditableLabel  *label,
			       const gchar       *str);
  void (* delete_from_cursor) (EelEditableLabel  *label,
			       GtkDeleteType      type,
			       gint               count);
  void (* cut_clipboard)      (EelEditableLabel  *label);
  void (* copy_clipboard)     (EelEditableLabel  *label);
  void (* paste_clipboard)    (EelEditableLabel  *label);
  void (* toggle_overwrite)   (EelEditableLabel  *label);
  
  /* Hook to customize right-click popup for selectable labels */
  void (* populate_popup)   (EelEditableLabel  *label,
                             GtkMenu           *menu);
};

GType                 eel_editable_label_get_type          (void) G_GNUC_CONST;
GtkWidget*            eel_editable_label_new                  (const char       *str);
void                  eel_editable_label_set_text             (EelEditableLabel *label,
							       const char       *str);
const gchar*          eel_editable_label_get_text             (EelEditableLabel *label);
void                  eel_editable_label_set_justify          (EelEditableLabel *label,
							       GtkJustification  jtype);
GtkJustification      eel_editable_label_get_justify          (EelEditableLabel *label);
void                  eel_editable_label_set_line_wrap        (EelEditableLabel *label,
							       gboolean          wrap);
void                  eel_editable_label_set_line_wrap_mode   (EelEditableLabel *label,
							       PangoWrapMode     mode);
gboolean              eel_editable_label_get_line_wrap        (EelEditableLabel *label);
void                  eel_editable_label_set_draw_outline     (EelEditableLabel *label,
							       gboolean          wrap);
void                  eel_editable_label_select_region        (EelEditableLabel *label,
							       gint              start_offset,
							       gint              end_offset);
gboolean              eel_editable_label_get_selection_bounds (EelEditableLabel *label,
							       gint             *start,
							       gint             *end);
PangoLayout *         eel_editable_label_get_layout           (EelEditableLabel *label);
void                  eel_editable_label_get_layout_offsets   (EelEditableLabel *label,
							       gint             *x,
							       gint             *y);
PangoFontDescription *eel_editable_label_get_font_description (EelEditableLabel *label);
void                  eel_editable_label_set_font_description (EelEditableLabel *label,
							       const PangoFontDescription *desc);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __EEL_EDITABLE_LABEL_H__ */
