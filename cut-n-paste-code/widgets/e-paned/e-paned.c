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

#include "e-paned.h"

enum {
  ARG_0,
  ARG_HANDLE_SIZE,
  ARG_QUANTUM,
};

static void    e_paned_class_init (EPanedClass  *klass);
static void    e_paned_init       (EPaned       *paned);
static void    e_paned_set_arg    (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);
static void    e_paned_get_arg    (GtkObject      *object,
				     GtkArg         *arg,
				     guint           arg_id);
static void    e_paned_realize    (GtkWidget      *widget);
static void    e_paned_map        (GtkWidget      *widget);
static void    e_paned_unmap      (GtkWidget      *widget);
static void    e_paned_unrealize  (GtkWidget      *widget);
static gint    e_paned_expose     (GtkWidget      *widget,
				     GdkEventExpose *event);
static void    e_paned_add        (GtkContainer   *container,
				     GtkWidget      *widget);
static void    e_paned_remove     (GtkContainer   *container,
				     GtkWidget      *widget);
static void    e_paned_forall     (GtkContainer   *container,
				     gboolean        include_internals,
				     GtkCallback     callback,
				     gpointer        callback_data);
static GtkType e_paned_child_type (GtkContainer   *container);

static GtkContainerClass *parent_class = NULL;


GtkType
e_paned_get_type (void)
{
  static GtkType paned_type = 0;
  
  if (!paned_type)
    {
      static const GtkTypeInfo paned_info =
      {
	"EPaned",
	sizeof (EPaned),
	sizeof (EPanedClass),
	(GtkClassInitFunc) e_paned_class_init,
	(GtkObjectInitFunc) e_paned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      paned_type = gtk_type_unique (GTK_TYPE_CONTAINER, &paned_info);
    }
  
  return paned_type;
}

static void
e_paned_class_init (EPanedClass *klass)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;
  GtkContainerClass *container_class;

  object_class = (GtkObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  container_class = (GtkContainerClass *) klass;

  parent_class = gtk_type_class (GTK_TYPE_CONTAINER);

  object_class->set_arg = e_paned_set_arg;
  object_class->get_arg = e_paned_get_arg;

  widget_class->realize = e_paned_realize;
  widget_class->map = e_paned_map;
  widget_class->unmap = e_paned_unmap;
  widget_class->unrealize = e_paned_unrealize;
  widget_class->expose_event = e_paned_expose;
  
  container_class->add = e_paned_add;
  container_class->remove = e_paned_remove;
  container_class->forall = e_paned_forall;
  container_class->child_type = e_paned_child_type;

  klass->handle_shown = NULL;

  gtk_object_add_arg_type("EPaned::handle_size", GTK_TYPE_UINT,
			  GTK_ARG_READWRITE, ARG_HANDLE_SIZE);
  gtk_object_add_arg_type("EPaned::quantum", GTK_TYPE_UINT,
			  GTK_ARG_READWRITE, ARG_QUANTUM);
}

static GtkType
e_paned_child_type (GtkContainer *container)
{
  if (!E_PANED (container)->child1 || !E_PANED (container)->child2)
    return GTK_TYPE_WIDGET;
  else
    return GTK_TYPE_NONE;
}

static void
e_paned_init (EPaned *paned)
{
  GTK_WIDGET_UNSET_FLAGS (paned, GTK_NO_WINDOW);
  
  paned->child1 = NULL;
  paned->child2 = NULL;
  paned->handle = NULL;
  paned->xor_gc = NULL;
  paned->cursor_type = GDK_CROSS;
  
  paned->handle_width = 5;
  paned->handle_height = 5;
  paned->handle_size = 5;
  paned->position_set = FALSE;
  paned->last_allocation = -1;
  paned->in_drag = FALSE;
  
  paned->handle_xpos = -1;
  paned->handle_ypos = -1;
  
  paned->old_child1_size = 0;
  paned->quantum = 1;
}

static void
e_paned_set_arg (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id)
{
  EPaned *paned = E_PANED (object);
  
  switch (arg_id)
    {
    case ARG_HANDLE_SIZE:
      e_paned_set_handle_size (paned, GTK_VALUE_UINT (*arg));
      break;
    case ARG_QUANTUM:
      paned->quantum = GTK_VALUE_UINT (*arg);
      if (paned->quantum == 0)
	paned->quantum = 1;
      break;
    default:
      break;
    }
}

static void
e_paned_get_arg (GtkObject *object,
		   GtkArg    *arg,
		   guint      arg_id)
{
  EPaned *paned = E_PANED (object);

  switch (arg_id)
    {
    case ARG_HANDLE_SIZE:
      GTK_VALUE_UINT (*arg) = paned->handle_size;
      break;
    case ARG_QUANTUM:
      GTK_VALUE_UINT (*arg) = paned->quantum;
      break;
    default:
      arg->type = GTK_TYPE_INVALID;
      break;
    }
}

static void
e_paned_realize (GtkWidget *widget)
{
  EPaned *paned;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_PANED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  paned = E_PANED (widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);
  attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

  widget->window = gdk_window_new (gtk_widget_get_parent_window(widget),
				   &attributes, attributes_mask);
  gdk_window_set_user_data (widget->window, paned);

  attributes.x = paned->handle_xpos;
  attributes.y = paned->handle_ypos;
  attributes.width = paned->handle_width;
  attributes.height = paned->handle_height;
  attributes.cursor = gdk_cursor_new (paned->cursor_type);
  attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
			    GDK_BUTTON_RELEASE_MASK |
			    GDK_POINTER_MOTION_MASK |
			    GDK_POINTER_MOTION_HINT_MASK);
  attributes_mask |= GDK_WA_CURSOR;

  paned->handle = gdk_window_new (widget->window,
				  &attributes, attributes_mask);
  gdk_window_set_user_data (paned->handle, paned);
  gdk_cursor_destroy (attributes.cursor);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
  gtk_style_set_background (widget->style, paned->handle, GTK_STATE_NORMAL);

  gdk_window_set_back_pixmap (widget->window, NULL, TRUE);

  if (e_paned_handle_shown(paned))
    gdk_window_show (paned->handle);
}

static void
e_paned_map (GtkWidget *widget)
{
  EPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_PANED (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_MAPPED);
  paned = E_PANED (widget);

  if (paned->child1 &&
      GTK_WIDGET_VISIBLE (paned->child1) &&
       !GTK_WIDGET_MAPPED (paned->child1))
    gtk_widget_map (paned->child1);
  if (paned->child2 &&
      GTK_WIDGET_VISIBLE (paned->child2) &&
      !GTK_WIDGET_MAPPED (paned->child2))
    gtk_widget_map (paned->child2);

  gdk_window_show (widget->window);
}

static void
e_paned_unmap (GtkWidget *widget)
{
  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_PANED (widget));

  GTK_WIDGET_UNSET_FLAGS (widget, GTK_MAPPED);

  gdk_window_hide (widget->window);
}

static void
e_paned_unrealize (GtkWidget *widget)
{
  EPaned *paned;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_PANED (widget));

  paned = E_PANED (widget);

  if (paned->xor_gc)
    {
      gdk_gc_destroy (paned->xor_gc);
      paned->xor_gc = NULL;
    }

  if (paned->handle)
    {
      gdk_window_set_user_data (paned->handle, NULL);
      gdk_window_destroy (paned->handle);
      paned->handle = NULL;
    }

  if (GTK_WIDGET_CLASS (parent_class)->unrealize)
    (* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static gint
e_paned_expose (GtkWidget      *widget,
		  GdkEventExpose *event)
{
  EPaned *paned;
  GdkEventExpose child_event;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (E_IS_PANED (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (GTK_WIDGET_DRAWABLE (widget))
    {
      paned = E_PANED (widget);

      if (paned->handle && event->window == paned->handle)
	{
	  if (e_paned_handle_shown(paned))
	    {
	      child_event = *event;
	      event->area.x += paned->handle_xpos;
	      event->area.y += paned->handle_ypos;
	      gtk_widget_draw (widget, &event->area);
	    }
	}
      else
	{
	  child_event = *event;
	  if (paned->child1 &&
	      GTK_WIDGET_NO_WINDOW (paned->child1) &&
	      gtk_widget_intersect (paned->child1, &event->area, &child_event.area))
	    gtk_widget_event (paned->child1, (GdkEvent *) &child_event);

	  if (paned->child2 &&
	      GTK_WIDGET_NO_WINDOW (paned->child2) &&
	      gtk_widget_intersect (paned->child2, &event->area, &child_event.area))
	    gtk_widget_event (paned->child2, (GdkEvent *) &child_event);
	}
    }

  return FALSE;
}

void
e_paned_add1 (EPaned  *paned,
		GtkWidget *widget)
{
  e_paned_pack1 (paned, widget, FALSE, TRUE);
}

void
e_paned_add2 (EPaned  *paned,
		GtkWidget *widget)
{
  e_paned_pack2 (paned, widget, TRUE, TRUE);
}

void
e_paned_pack1 (EPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (E_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child1)
    {
      paned->child1 = child;
      paned->child1_resize = resize;
      paned->child1_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));

      if (GTK_WIDGET_REALIZED (child->parent))
	gtk_widget_realize (child);

      if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child))
	{
	  if (GTK_WIDGET_MAPPED (child->parent))
	    gtk_widget_map (child);

	  gtk_widget_queue_resize (child);
	}
    }
}

void
e_paned_pack2 (EPaned  *paned,
		 GtkWidget *child,
		 gboolean   resize,
		 gboolean   shrink)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (E_IS_PANED (paned));
  g_return_if_fail (GTK_IS_WIDGET (child));

  if (!paned->child2)
    {
      paned->child2 = child;
      paned->child2_resize = resize;
      paned->child2_shrink = shrink;

      gtk_widget_set_parent (child, GTK_WIDGET (paned));

      if (GTK_WIDGET_REALIZED (child->parent))
	gtk_widget_realize (child);

      if (GTK_WIDGET_VISIBLE (child->parent) && GTK_WIDGET_VISIBLE (child))
	{
	  if (GTK_WIDGET_MAPPED (child->parent))
	    gtk_widget_map (child);

	  gtk_widget_queue_resize (child);
	}
    }
}


static void
e_paned_add (GtkContainer *container,
	       GtkWidget    *widget)
{
  EPaned *paned;

  g_return_if_fail (container != NULL);
  g_return_if_fail (E_IS_PANED (container));
  g_return_if_fail (widget != NULL);

  paned = E_PANED (container);

  if (!paned->child1)
    e_paned_add1 (E_PANED (container), widget);
  else if (!paned->child2)
    e_paned_add2 (E_PANED (container), widget);
}

static void
e_paned_remove (GtkContainer *container,
		  GtkWidget    *widget)
{
  EPaned *paned;
  gboolean was_visible;

  g_return_if_fail (container != NULL);
  g_return_if_fail (E_IS_PANED (container));
  g_return_if_fail (widget != NULL);

  paned = E_PANED (container);
  was_visible = GTK_WIDGET_VISIBLE (widget);

  if (paned->child1 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child1 = NULL;

      if (was_visible && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
  else if (paned->child2 == widget)
    {
      gtk_widget_unparent (widget);

      paned->child2 = NULL;

      if (was_visible && GTK_WIDGET_VISIBLE (container))
	gtk_widget_queue_resize (GTK_WIDGET (container));
    }
}

static void
e_paned_forall (GtkContainer *container,
		  gboolean      include_internals,
		  GtkCallback   callback,
		  gpointer      callback_data)
{
  EPaned *paned;

  g_return_if_fail (container != NULL);
  g_return_if_fail (E_IS_PANED (container));
  g_return_if_fail (callback != NULL);

  paned = E_PANED (container);

  if (paned->child1)
    (*callback) (paned->child1, callback_data);
  if (paned->child2)
    (*callback) (paned->child2, callback_data);
}

gint
e_paned_get_position (EPaned  *paned)
{
  g_return_val_if_fail (paned != NULL, 0);
  g_return_val_if_fail (E_IS_PANED (paned), 0);

  return paned->child1_size;
}

void
e_paned_set_position (EPaned *paned,
			gint      position)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (E_IS_PANED (paned));

  if (position >= 0)
    {
      /* We don't clamp here - the assumption is that
       * if the total allocation changes at the same time
       * as the position, the position set is with reference
       * to the new total size. If only the position changes,
       * then clamping will occur in e_paned_compute_position()
       */
      paned->child1_size = position;
      paned->position_set = TRUE;
    }
  else
    {
      paned->position_set = FALSE;
    }

  gtk_widget_queue_resize (GTK_WIDGET (paned));
}

void
e_paned_set_handle_size (EPaned *paned,
			   guint16   size)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (E_IS_PANED (paned));

  gtk_widget_queue_resize (GTK_WIDGET (paned));

  paned->handle_size = size;
}

void
e_paned_compute_position(EPaned *paned,
			 gint      allocation,
			 gint      child1_req,
			 gint      child2_req)
{
  g_return_if_fail (paned != NULL);
  g_return_if_fail (E_IS_PANED (paned));
  
  if (e_paned_handle_shown(paned))
    allocation -= (gint) paned->handle_size;

  paned->min_position = paned->child1_shrink ? 0 : child1_req;

  paned->max_position = allocation;
  if (!paned->child2_shrink)
    paned->max_position = MAX (1, paned->max_position - child2_req);

  if (!paned->position_set)
    {
      if (paned->child1_resize && !paned->child2_resize)
	paned->child1_size = MAX (1, allocation - child2_req);
      else if (!paned->child1_resize && paned->child2_resize)
	paned->child1_size = child1_req;
      else if (child1_req + child2_req != 0)
	paned->child1_size = allocation * ((gdouble)child1_req / (child1_req + child2_req));
      else
	paned->child1_size = allocation * 0.5;
    }
  else
    {
      /* If the position was set before the initial allocation.
       * (paned->last_allocation <= 0) just clamp it and leave it.
       */
      if (paned->last_allocation > 0)
	{
	  if (paned->child1_resize && !paned->child2_resize)
	    paned->child1_size += allocation - paned->last_allocation;
	  else if (!(!paned->child1_resize && paned->child2_resize))
	    paned->child1_size = allocation * ((gdouble) paned->child1_size / (paned->last_allocation));
	}
    }

  paned->child1_size = CLAMP (paned->child1_size,
			      paned->min_position,
			      paned->max_position);

  paned->last_allocation = allocation;
}

gboolean
e_paned_handle_shown(EPaned *paned)
{
  EPanedClass *klass = E_PANED_CLASS(GTK_OBJECT(paned)->klass);
  if (klass->handle_shown)
    return (*klass->handle_shown)(paned);
  else
    return TRUE;
}

gint
e_paned_quantized_size(EPaned *paned,
		       gint    size)
{
  gint quantization = size - paned->old_child1_size;
  if (quantization > 0)
    quantization += paned->quantum / 2;
  else
    quantization -= paned->quantum / 2;
  quantization /= paned->quantum;
  quantization *= paned->quantum;
  return paned->old_child1_size + quantization;
}
