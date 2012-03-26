/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#ifndef __NAUTILUS_TRASH_BAR_H
#define __NAUTILUS_TRASH_BAR_H

#include "nautilus-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TRASH_BAR         (nautilus_trash_bar_get_type ())
#define NAUTILUS_TRASH_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBar))
#define NAUTILUS_TRASH_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBarClass))
#define NAUTILUS_IS_TRASH_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_TRASH_BAR))
#define NAUTILUS_IS_TRASH_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_TRASH_BAR))
#define NAUTILUS_TRASH_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_TRASH_BAR, NautilusTrashBarClass))

typedef struct NautilusTrashBarPrivate NautilusTrashBarPrivate;

typedef struct
{
	GtkInfoBar parent;

	NautilusTrashBarPrivate *priv;
} NautilusTrashBar;

typedef struct
{
	GtkInfoBarClass parent_class;
} NautilusTrashBarClass;

GType		 nautilus_trash_bar_get_type	(void) G_GNUC_CONST;

GtkWidget       *nautilus_trash_bar_new         (NautilusView *view);


G_END_DECLS

#endif /* __GS_TRASH_BAR_H */
