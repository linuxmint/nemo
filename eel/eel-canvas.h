/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1997, 1998, 1999, 2000 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */
/* EelCanvas widget - Tk-like canvas widget for Gnome
 *
 * EelCanvas is basically a port of the Tk toolkit's most excellent canvas
 * widget.  Tk is copyrighted by the Regents of the University of California,
 * Sun Microsystems, and other parties.
 *
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Raph Levien <raph@gimp.org>
 */

#ifndef EEL_CANVAS_H
#define EEL_CANVAS_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <stdarg.h>

G_BEGIN_DECLS


/* "Small" value used by canvas stuff */
#define EEL_CANVAS_EPSILON 1e-10


/* Macros for building colors that fit in a 32-bit integer.  The values are in
 * [0, 255].
 */

#define EEL_CANVAS_COLOR(r, g, b) ((((int) (r) & 0xff) << 24)	\
				     | (((int) (g) & 0xff) << 16)	\
				     | (((int) (b) & 0xff) << 8)	\
				     | 0xff)

#define EEL_CANVAS_COLOR_A(r, g, b, a) ((((int) (r) & 0xff) << 24)	\
					  | (((int) (g) & 0xff) << 16)	\
					  | (((int) (b) & 0xff) << 8)	\
					  | ((int) (a) & 0xff))


typedef struct _EelCanvas           EelCanvas;
typedef struct _EelCanvasClass      EelCanvasClass;
typedef struct _EelCanvasItem       EelCanvasItem;
typedef struct _EelCanvasItemClass  EelCanvasItemClass;
typedef struct _EelCanvasGroup      EelCanvasGroup;
typedef struct _EelCanvasGroupClass EelCanvasGroupClass;


/* EelCanvasItem - base item class for canvas items
 *
 * All canvas items are derived from EelCanvasItem.  The only information a
 * EelCanvasItem contains is its parent canvas, its parent canvas item group,
 * and its bounding box in world coordinates.
 *
 * Items inside a canvas are organized in a tree of EelCanvasItemGroup nodes
 * and EelCanvasItem leaves.  Each canvas has a single root group, which can
 * be obtained with the eel_canvas_get_root() function.
 *
 * The abstract EelCanvasItem class does not have any configurable or
 * queryable attributes.
 */

/* Object flags for items */
enum {
	EEL_CANVAS_ITEM_REALIZED         = 1 << 4,
	EEL_CANVAS_ITEM_MAPPED           = 1 << 5,
	EEL_CANVAS_ITEM_ALWAYS_REDRAW    = 1 << 6,
	EEL_CANVAS_ITEM_VISIBLE          = 1 << 7,
	EEL_CANVAS_ITEM_NEED_UPDATE      = 1 << 8,
	EEL_CANVAS_ITEM_NEED_DEEP_UPDATE = 1 << 9
};

/* Update flags for items */
enum {
	EEL_CANVAS_UPDATE_REQUESTED  = 1 << 0,
	EEL_CANVAS_UPDATE_DEEP       = 1 << 1
};

#define EEL_TYPE_CANVAS_ITEM            (eel_canvas_item_get_type ())
#define EEL_CANVAS_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS_ITEM, EelCanvasItem))
#define EEL_CANVAS_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS_ITEM, EelCanvasItemClass))
#define EEL_IS_CANVAS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS_ITEM))
#define EEL_IS_CANVAS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS_ITEM))
#define EEL_CANVAS_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS_ITEM, EelCanvasItemClass))


struct _EelCanvasItem {
	GInitiallyUnowned object;

	/* Parent canvas for this item */
	EelCanvas *canvas;

	/* Parent canvas group for this item (a EelCanvasGroup) */
	EelCanvasItem *parent;

	/* Bounding box for this item (in canvas coordinates) */
	double x1, y1, x2, y2;

	/* Object flags */
	guint flags;
};

struct _EelCanvasItemClass {
	GInitiallyUnownedClass parent_class;

	void (* destroy) (EelCanvasItem *item);

	/* Tell the item to update itself.  The flags are from the update flags
	 * defined above.  The item should update its internal state from its
	 * queued state, and recompute and request its repaint area. The
	 * update method also recomputes the bounding box of the item.
	 */
	void (* update) (EelCanvasItem *item, double i2w_dx, double i2w_dy, int flags);

	/* Realize an item -- create GCs, etc. */
	void (* realize) (EelCanvasItem *item);

	/* Unrealize an item */
	void (* unrealize) (EelCanvasItem *item);

	/* Map an item - normally only need by items with their own GdkWindows */
	void (* map) (EelCanvasItem *item);

	/* Unmap an item */
	void (* unmap) (EelCanvasItem *item);

	/* Draw an item of this type.  (x, y) are the upper-left canvas pixel
	 * coordinates of the drawable, a temporary pixmap, where things get
	 * drawn.  (width, height) are the dimensions of the drawable.
	 */
	void (* draw) (EelCanvasItem *item, cairo_t *cr, cairo_region_t *region);

	/* Calculate the distance from an item to the specified point.  It also
         * returns a canvas item which is the item itself in the case of the
         * object being an actual leaf item, or a child in case of the object
         * being a canvas group.  (cx, cy) are the canvas pixel coordinates that
         * correspond to the item-relative coordinates (x, y).
	 */
	double (* point) (EelCanvasItem *item, double x, double y, int cx, int cy,
			  EelCanvasItem **actual_item);

	void (* translate) (EelCanvasItem *item, double dx, double dy);
	
	/* Fetch the item's bounding box (need not be exactly tight).  This
	 * should be in item-relative coordinates.
	 */
	void (* bounds) (EelCanvasItem *item, double *x1, double *y1, double *x2, double *y2);

	/* Signal: an event ocurred for an item of this type.  The (x, y)
	 * coordinates are in the canvas world coordinate system.
	 */
	gboolean (* event)                (EelCanvasItem *item, GdkEvent *event);

	/* Reserved for future expansion */
	gpointer spare_vmethods [4];
};


/* Standard Gtk function */
GType eel_canvas_item_get_type (void) G_GNUC_CONST;

/* Create a canvas item using the standard Gtk argument mechanism.  The item is
 * automatically inserted at the top of the specified canvas group.  The last
 * argument must be a NULL pointer.
 */
EelCanvasItem *eel_canvas_item_new (EelCanvasGroup *parent, GType type,
				    const gchar *first_arg_name, ...);

void eel_canvas_item_destroy (EelCanvasItem *item);

/* Constructors for use in derived classes and language wrappers */
void eel_canvas_item_construct (EelCanvasItem *item, EelCanvasGroup *parent,
				const gchar *first_arg_name, va_list args);

/* Configure an item using the standard Gtk argument mechanism.  The last
 * argument must be a NULL pointer.
 */
void eel_canvas_item_set (EelCanvasItem *item, const gchar *first_arg_name, ...);

/* Used only for language wrappers and the like */
void eel_canvas_item_set_valist (EelCanvasItem *item,
				 const gchar *first_arg_name, va_list args);

/* Move an item by the specified amount */
void eel_canvas_item_move (EelCanvasItem *item, double dx, double dy);

/* Raise an item in the z-order of its parent group by the specified number of
 * positions.
 */
void eel_canvas_item_raise (EelCanvasItem *item, int positions);

/* Lower an item in the z-order of its parent group by the specified number of
 * positions.
 */
void eel_canvas_item_lower (EelCanvasItem *item, int positions);

/* Raise an item to the top of its parent group's z-order. */
void eel_canvas_item_raise_to_top (EelCanvasItem *item);

/* Lower an item to the bottom of its parent group's z-order */
void eel_canvas_item_lower_to_bottom (EelCanvasItem *item);

/* Send an item behind another item */
void eel_canvas_item_send_behind (EelCanvasItem *item,
				  EelCanvasItem *behind_item);


/* Show an item (make it visible).  If the item is already shown, it has no
 * effect.
 */
void eel_canvas_item_show (EelCanvasItem *item);

/* Hide an item (make it invisible).  If the item is already invisible, it has
 * no effect.
 */
void eel_canvas_item_hide (EelCanvasItem *item);

/* Grab the mouse for the specified item.  Only the events in event_mask will be
 * reported.  If cursor is non-NULL, it will be used during the duration of the
 * grab.  Time is a proper X event time parameter.  Returns the same values as
 * XGrabPointer().
 */
GdkGrabStatus eel_canvas_item_grab (EelCanvasItem *item,
				    GdkEventMask event_mask,
				    GdkCursor *cursor,
				    guint32 etime);

/* Ungrabs the mouse -- the specified item must be the same that was passed to
 * eel_canvas_item_grab().  Time is a proper X event time parameter.
 */
void eel_canvas_item_ungrab (EelCanvasItem *item, guint32 etime);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates and "i" is item coordinates.
 */
void eel_canvas_item_w2i (EelCanvasItem *item, double *x, double *y);
void eel_canvas_item_i2w (EelCanvasItem *item, double *x, double *y);

/* Remove the item from its parent group and make the new group its parent.  The
 * item will be put on top of all the items in the new group.  The item's
 * coordinates relative to its new parent to *not* change -- this means that the
 * item could potentially move on the screen.
 * 
 * The item and the group must be in the same canvas.  An item cannot be
 * reparented to a group that is the item itself or that is an inferior of the
 * item.
 */
void eel_canvas_item_reparent (EelCanvasItem *item, EelCanvasGroup *new_group);

/* Used to send all of the keystroke events to a specific item as well as
 * GDK_FOCUS_CHANGE events.
 */
void eel_canvas_item_grab_focus (EelCanvasItem *item);

/* Fetch the bounding box of the item.  The bounding box may not be exactly
 * tight, but the canvas items will do the best they can.  The returned bounding
 * box is in the coordinate system of the item's parent.
 */
void eel_canvas_item_get_bounds (EelCanvasItem *item,
				 double *x1, double *y1, double *x2, double *y2);

/* Request that the update method eventually get called.  This should be used
 * only by item implementations.
 */
void eel_canvas_item_request_update (EelCanvasItem *item);

/* Request a redraw of the bounding box of the canvas item */
void eel_canvas_item_request_redraw (EelCanvasItem *item);

/* EelCanvasGroup - a group of canvas items
 *
 * A group is a node in the hierarchical tree of groups/items inside a canvas.
 * Groups serve to give a logical structure to the items.
 *
 * Consider a circuit editor application that uses the canvas for its schematic
 * display.  Hierarchically, there would be canvas groups that contain all the
 * components needed for an "adder", for example -- this includes some logic
 * gates as well as wires.  You can move stuff around in a convenient way by
 * doing a eel_canvas_item_move() of the hierarchical groups -- to move an
 * adder, simply move the group that represents the adder.
 *
 * The following arguments are available:
 *
 * name		type		read/write	description
 * --------------------------------------------------------------------------------
 * x		double		RW		X coordinate of group's origin
 * y		double		RW		Y coordinate of group's origin
 */


#define EEL_TYPE_CANVAS_GROUP            (eel_canvas_group_get_type ())
#define EEL_CANVAS_GROUP(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS_GROUP, EelCanvasGroup))
#define EEL_CANVAS_GROUP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS_GROUP, EelCanvasGroupClass))
#define EEL_IS_CANVAS_GROUP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS_GROUP))
#define EEL_IS_CANVAS_GROUP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS_GROUP))
#define EEL_CANVAS_GROUP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS_GROUP, EelCanvasGroupClass))


struct _EelCanvasGroup {
	EelCanvasItem item;

	double xpos, ypos;
	
	/* Children of the group */
	GList *item_list;
	GList *item_list_end;
};

struct _EelCanvasGroupClass {
	EelCanvasItemClass parent_class;
};


/* Standard Gtk function */
GType eel_canvas_group_get_type (void) G_GNUC_CONST;


/*** EelCanvas ***/


#define EEL_TYPE_CANVAS            (eel_canvas_get_type ())
#define EEL_CANVAS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EEL_TYPE_CANVAS, EelCanvas))
#define EEL_CANVAS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EEL_TYPE_CANVAS, EelCanvasClass))
#define EEL_IS_CANVAS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EEL_TYPE_CANVAS))
#define EEL_IS_CANVAS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EEL_TYPE_CANVAS))
#define EEL_CANVAS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EEL_TYPE_CANVAS, EelCanvasClass))


struct _EelCanvas {
	GtkLayout layout;

	/* Root canvas group */
	EelCanvasItem *root;

	/* The item containing the mouse pointer, or NULL if none */
	EelCanvasItem *current_item;

	/* Item that is about to become current (used to track deletions and such) */
	EelCanvasItem *new_current_item;

	/* Item that holds a pointer grab, or NULL if none */
	EelCanvasItem *grabbed_item;

	/* If non-NULL, the currently focused item */
	EelCanvasItem *focused_item;

	/* Event on which selection of current item is based */
	GdkEvent pick_event;

	/* Scrolling region */
	double scroll_x1, scroll_y1;
	double scroll_x2, scroll_y2;

	/* Scaling factor to be used for display */
	double pixels_per_unit;

	/* Idle handler ID */
	guint idle_id;

	/* Signal handler ID for destruction of the root item */
	guint root_destroy_id;

	/* Internal pixel offsets when zoomed out */
	int zoom_xofs, zoom_yofs;

	/* Last known modifier state, for deferred repick when a button is down */
	int state;

	/* Event mask specified when grabbing an item */
	guint grabbed_event_mask;

	/* Tolerance distance for picking items */
	int close_enough;

	/* Whether the canvas should center the canvas in the middle of
	 * the window if the scroll region is smaller than the window */
	unsigned int center_scroll_region : 1;

	/* Whether items need update at next idle loop iteration */
	unsigned int need_update : 1;

	/* Are we in the midst of an update */
	unsigned int doing_update : 1;

	/* Whether the canvas needs redrawing at the next idle loop iteration */
	unsigned int need_redraw : 1;

	/* Whether current item will be repicked at next idle loop iteration */
	unsigned int need_repick : 1;

	/* For use by internal pick_current_item() function */
	unsigned int left_grabbed_item : 1;

	/* For use by internal pick_current_item() function */
	unsigned int in_repick : 1;
};

struct _EelCanvasClass {
	GtkLayoutClass parent_class;

	/* Draw the background for the area given.
	 */
	void (* draw_background) (EelCanvas *canvas,
                                  cairo_t *cr);

	/* Private Virtual methods for groping the canvas inside bonobo */
	void (* request_update) (EelCanvas *canvas);

	/* Reserved for future expansion */
	gpointer spare_vmethods [4];
};


/* Standard Gtk function */
GType eel_canvas_get_type (void) G_GNUC_CONST;

/* Creates a new canvas.  You should check that the canvas is created with the
 * proper visual and colormap.  Any visual will do unless you intend to insert
 * gdk_imlib images into it, in which case you should use the gdk_imlib visual.
 *
 * You should call eel_canvas_set_scroll_region() soon after calling this
 * function to set the desired scrolling limits for the canvas.
 */
GtkWidget *eel_canvas_new (void);

/* Returns the root canvas item group of the canvas */
EelCanvasGroup *eel_canvas_root (EelCanvas *canvas);

/* Sets the limits of the scrolling region, in world coordinates */
void eel_canvas_set_scroll_region (EelCanvas *canvas,
				   double x1, double y1, double x2, double y2);

/* Gets the limits of the scrolling region, in world coordinates */
void eel_canvas_get_scroll_region (EelCanvas *canvas,
				   double *x1, double *y1, double *x2, double *y2);

/* Sets the number of pixels that correspond to one unit in world coordinates */
void eel_canvas_set_pixels_per_unit (EelCanvas *canvas, double n);

/* Wether the canvas centers the scroll region if it is smaller than the window  */
void eel_canvas_set_center_scroll_region (EelCanvas *canvas, gboolean center_scroll_region);

/* Scrolls the canvas to the specified offsets, given in canvas pixel coordinates */
void eel_canvas_scroll_to (EelCanvas *canvas, int cx, int cy);

/* Returns the scroll offsets of the canvas in canvas pixel coordinates.  You
 * can specify NULL for any of the values, in which case that value will not be
 * queried.
 */
void eel_canvas_get_scroll_offsets (EelCanvas *canvas, int *cx, int *cy);

/* Requests that the canvas be repainted immediately instead of in the idle
 * loop.
 */
void eel_canvas_update_now (EelCanvas *canvas);

/* Returns the item that is at the specified position in world coordinates, or
 * NULL if no item is there.
 */
EelCanvasItem *eel_canvas_get_item_at (EelCanvas *canvas, double x, double y);

/* For use only by item type implementations.  Request that the canvas
 * eventually redraw the specified region, specified in canvas pixel
 * coordinates.  The region contains (x1, y1) but not (x2, y2).
 */
void eel_canvas_request_redraw (EelCanvas *canvas, int x1, int y1, int x2, int y2);

/* These functions convert from a coordinate system to another.  "w" is world
 * coordinates, "c" is canvas pixel coordinates (pixel coordinates that are
 * (0,0) for the upper-left scrolling limit and something else for the
 * lower-left scrolling limit).
 */
void eel_canvas_w2c_rect_d (EelCanvas *canvas,
			    double *x1, double *y1,
			    double *x2, double *y2);
void eel_canvas_w2c (EelCanvas *canvas, double wx, double wy, int *cx, int *cy);
void eel_canvas_w2c_d (EelCanvas *canvas, double wx, double wy, double *cx, double *cy);
void eel_canvas_c2w (EelCanvas *canvas, int cx, int cy, double *wx, double *wy);

/* This function takes in coordinates relative to the GTK_LAYOUT
 * (canvas)->bin_window and converts them to world coordinates.
 * These days canvas coordinates and window coordinates are the same, but
 * these are left for backwards compat reasons.
 */
void eel_canvas_window_to_world (EelCanvas *canvas,
				 double winx, double winy, double *worldx, double *worldy);

/* This is the inverse of eel_canvas_window_to_world() */
void eel_canvas_world_to_window (EelCanvas *canvas,
				 double worldx, double worldy, double *winx, double *winy);

/* Accessible implementation */
GType eel_canvas_accessible_get_type(void);

typedef struct _EelCanvasAccessible EelCanvasAccessible;
struct _EelCanvasAccessible
{
	GtkAccessible parent;
};

typedef struct _EelCanvasAccessibleClass EelCanvasAccessibleClass;
struct _EelCanvasAccessibleClass
{
	GtkAccessibleClass parent_class;
};

G_END_DECLS

#endif
