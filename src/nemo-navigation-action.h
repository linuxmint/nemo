/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nemo
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  Nemo is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nemo is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *
 */

#ifndef NEMO_NAVIGATION_ACTION_H
#define NEMO_NAVIGATION_ACTION_H

#include <gtk/gtk.h>

#define NEMO_TYPE_NAVIGATION_ACTION            (nemo_navigation_action_get_type ())
#define NEMO_NAVIGATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_NAVIGATION_ACTION, NemoNavigationAction))
#define NEMO_NAVIGATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_NAVIGATION_ACTION, NemoNavigationActionClass))
#define NEMO_IS_NAVIGATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_NAVIGATION_ACTION))
#define NEMO_IS_NAVIGATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NEMO_TYPE_NAVIGATION_ACTION))
#define NEMO_NAVIGATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NEMO_TYPE_NAVIGATION_ACTION, NemoNavigationActionClass))

typedef struct _NemoNavigationAction       NemoNavigationAction;
typedef struct _NemoNavigationActionClass  NemoNavigationActionClass;
typedef struct NemoNavigationActionPrivate NemoNavigationActionPrivate;

typedef enum
{
    NEMO_NAVIGATION_DIRECTION_BACK,
    NEMO_NAVIGATION_DIRECTION_FORWARD,
    NEMO_NAVIGATION_DIRECTION_UP,
    NEMO_NAVIGATION_DIRECTION_RELOAD,
    NEMO_NAVIGATION_DIRECTION_HOME,
    NEMO_NAVIGATION_DIRECTION_COMPUTER,
    NEMO_NAVIGATION_DIRECTION_EDIT,

} NemoNavigationDirection;

struct _NemoNavigationAction
{
	GtkAction parent;
	
	/*< private >*/
	NemoNavigationActionPrivate *priv;
};

struct _NemoNavigationActionClass
{
	GtkActionClass parent_class;
};

GType    nemo_navigation_action_get_type   (void);

#endif
