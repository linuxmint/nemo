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

#ifndef __E_VPANED_H__
#define __E_VPANED_H__

#include <widgets/e-paned/e-paned.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define E_TYPE_VPANED            (e_vpaned_get_type ())
#define E_VPANED(obj)            (GTK_CHECK_CAST ((obj), E_TYPE_VPANED, EVPaned))
#define E_VPANED_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_VPANED, EVPanedClass))
#define E_IS_VPANED(obj)         (GTK_CHECK_TYPE ((obj), E_TYPE_VPANED))
#define E_IS_VPANED_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_VPANED))
#define E_VPANED_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), E_TYPE_VPANED, EVPanedClass))


typedef struct _EVPaned      EVPaned;
typedef struct _EVPanedClass EVPanedClass;

struct _EVPaned
{
  EPaned paned;
};

struct _EVPanedClass
{
  EPanedClass parent_class;
};

GtkType    e_vpaned_get_type (void);
GtkWidget *e_vpaned_new      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_VPANED_H__ */
