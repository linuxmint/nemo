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

#ifndef __E_PANED_H__
#define __E_PANED_H__

#include <gtk/gtkcontainer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define E_TYPE_PANED                  (e_paned_get_type ())
#define E_PANED(obj)                  (GTK_CHECK_CAST ((obj), E_TYPE_PANED, EPaned))
#define E_PANED_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), E_TYPE_PANED, EPanedClass))
#define E_IS_PANED(obj)               (GTK_CHECK_TYPE ((obj), E_TYPE_PANED))
#define E_IS_PANED_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), E_TYPE_PANED))
#define E_PANED_GET_CLASS(obj)        (GTK_CHECK_GET_CLASS ((obj), E_TYPE_PANED, EPanedClass))


typedef struct _EPaned      EPaned;
typedef struct _EPanedClass EPanedClass;

struct _EPaned
{
  GtkContainer container;
  
  GtkWidget *child1;
  GtkWidget *child2;
  
  GdkWindow *handle;
  GdkGC *xor_gc;
  GdkCursorType cursor_type;
  
  /*< public >*/
  guint16 handle_size;
  
  /*< private >*/
  guint16 handle_width;
  guint16 handle_height;

  gint child1_size;
  gint last_allocation;
  gint min_position;
  gint max_position;

  gint old_child1_size;
  gint quantum;

  guint position_set : 1;
  guint in_drag : 1;
  guint child1_shrink : 1;
  guint child1_resize : 1;
  guint child2_shrink : 1;
  guint child2_resize : 1;

  gint16 handle_xpos;
  gint16 handle_ypos;
};

struct _EPanedClass
{
  GtkContainerClass parent_class;

  /* Protected virtual method. */
  gboolean (*handle_shown) (EPaned *paned);
};


GtkType e_paned_get_type        (void);
void    e_paned_add1            (EPaned    *paned,
				 GtkWidget *child);
void    e_paned_add2            (EPaned    *paned,
				 GtkWidget *child);
void    e_paned_pack1           (EPaned    *paned,
				 GtkWidget *child,
				 gboolean   resize,
				 gboolean   shrink);
void    e_paned_pack2           (EPaned    *paned,
				 GtkWidget *child,
				 gboolean   resize,
				 gboolean   shrink);
gint    e_paned_get_position    (EPaned    *paned);
void    e_paned_set_position    (EPaned    *paned,
				 gint       position);
void    e_paned_set_handle_size (EPaned    *paned,
				 guint16    size);

/* Internal function */
void    e_paned_compute_position (EPaned   *paned,
				  gint      allocation,
				  gint      child1_req,
				  gint      child2_req);

gboolean e_paned_handle_shown    (EPaned   *paned);
gint     e_paned_quantized_size  (EPaned   *paned,
				  int       size);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_PANED_H__ */
