/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* eel-accessibility.h - Utility functions for accessibility

   Copyright (C) 2002 Anders Carlsson

   The Eel Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Eel Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Eel Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#ifndef EEL_ACCESSIBILITY_H
#define EEL_ACCESSIBILITY_H

#include <glib-object.h>
#include <atk/atkobject.h>
#include <atk/atkregistry.h>
#include <atk/atkobjectfactory.h>
#include <gtk/gtk.h>
#include <libgail-util/gailtextutil.h>

void eel_accessibility_set_up_label_widget_relation (GtkWidget *label, GtkWidget *widget);

AtkObject    *eel_accessibility_get_atk_object        (gpointer              object);
gpointer      eel_accessibility_get_gobject           (AtkObject            *object);
AtkObject    *eel_accessibility_set_atk_object_return (gpointer              object,
						       AtkObject            *atk_object);
GType         eel_accessibility_create_accessible_gtype (const char *type_name,
							 GtkWidget *widget,
							 GClassInitFunc class_init);

char*         eel_accessibility_text_get_text         (AtkText              *text,
                                                       gint                 start_pos,
                                                       gint                 end_pos);
gunichar      eel_accessibility_text_get_character_at_offset
                                                      (AtkText              *text,
                                                       gint                 offset);
char*         eel_accessibility_text_get_text_before_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
char*         eel_accessibility_text_get_text_at_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
char*         eel_accessibility_text_get_text_after_offset
                                                      (AtkText              *text,
                                                       gint                 offset,
                                                       AtkTextBoundary      boundary_type,
                                                       gint                 *start_offset,
                                                       gint                 *end_offset);
gint          eel_accessibility_text_get_character_count
                                                      (AtkText              *text);

                     
#define EEL_TYPE_ACCESSIBLE_TEXT           (eel_accessible_text_get_type ())
#define EEL_IS_ACCESSIBLE_TEXT(obj)        G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_ACCESSIBLE_TEXT)
#define EEL_ACCESSIBLE_TEXT(obj)           G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_ACCESSIBLE_TEXT, EelAccessibleText)
#define EEL_ACCESSIBLE_TEXT_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), EEL_TYPE_ACCESSIBLE_TEXT, EelAccessibleTextIface))

/* Instead of implementing the AtkText interface, implement this */
typedef struct _EelAccessibleText EelAccessibleText;

typedef struct {
	GTypeInterface parent;
	
	GailTextUtil *(*get_text)   (GObject *text);
	PangoLayout  *(*get_layout) (GObject *text);
} EelAccessibleTextIface;

GType eel_accessible_text_get_type      (void);

#endif /* EEL_ACCESSIBILITY_H */
