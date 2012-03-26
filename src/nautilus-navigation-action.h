/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *
 */

#ifndef NAUTILUS_NAVIGATION_ACTION_H
#define NAUTILUS_NAVIGATION_ACTION_H

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_NAVIGATION_ACTION            (nautilus_navigation_action_get_type ())
#define NAUTILUS_NAVIGATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NAVIGATION_ACTION, NautilusNavigationAction))
#define NAUTILUS_NAVIGATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_NAVIGATION_ACTION, NautilusNavigationActionClass))
#define NAUTILUS_IS_NAVIGATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NAVIGATION_ACTION))
#define NAUTILUS_IS_NAVIGATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_NAVIGATION_ACTION))
#define NAUTILUS_NAVIGATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NAUTILUS_TYPE_NAVIGATION_ACTION, NautilusNavigationActionClass))

typedef struct _NautilusNavigationAction       NautilusNavigationAction;
typedef struct _NautilusNavigationActionClass  NautilusNavigationActionClass;
typedef struct NautilusNavigationActionPrivate NautilusNavigationActionPrivate;

typedef enum
{
	NAUTILUS_NAVIGATION_DIRECTION_BACK,
	NAUTILUS_NAVIGATION_DIRECTION_FORWARD
} NautilusNavigationDirection;

struct _NautilusNavigationAction
{
	GtkAction parent;
	
	/*< private >*/
	NautilusNavigationActionPrivate *priv;
};

struct _NautilusNavigationActionClass
{
	GtkActionClass parent_class;
};

GType    nautilus_navigation_action_get_type   (void);

#endif
