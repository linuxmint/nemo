/* EPaned - A slightly more advanced paned widget.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Christopher James Lahey <clahey@helixcode.com>
 *
 * based on GtkPaned from Gtk+.  Gtk+ Copyright notice follows.
 */

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __E_HPANED_H__
#define __E_HPANED_H__

#include <widgets/e-paned/e-paned.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TYPE_HPANED	         (e_hpaned_get_type ())
#define E_HPANED(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_HPANED, EHPaned))
#define E_HPANED_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_HPANED, EHPanedClass))
#define E_IS_HPANED(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_HPANED))
#define E_IS_HPANED_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_HPANED))
#define E_HPANED_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_HPANED, EHPanedClass))


typedef struct _EHPaned      EHPaned;
typedef struct _EHPanedClass EHPanedClass;

struct _EHPaned
{
  EPaned paned;
};

struct _EHPanedClass
{
  EPanedClass parent_class;
};

GtkType    e_hpaned_get_type (void);
GtkWidget *e_hpaned_new      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_HPANED_H__ */
