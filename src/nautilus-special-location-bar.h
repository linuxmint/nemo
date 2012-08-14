/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
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
 */

#ifndef __NAUTILUS_SPECIAL_LOCATION_BAR_H
#define __NAUTILUS_SPECIAL_LOCATION_BAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SPECIAL_LOCATION_BAR         (nautilus_special_location_bar_get_type ())
#define NAUTILUS_SPECIAL_LOCATION_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_SPECIAL_LOCATION_BAR, NautilusSpecialLocationBar))
#define NAUTILUS_SPECIAL_LOCATION_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_SPECIAL_LOCATION_BAR, NautilusSpecialLocationBarClass))
#define NAUTILUS_IS_SPECIAL_LOCATION_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_SPECIAL_LOCATION_BAR))
#define NAUTILUS_IS_SPECIAL_LOCATION_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_SPECIAL_LOCATION_BAR))
#define NAUTILUS_SPECIAL_LOCATION_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_SPECIAL_LOCATION_BAR, NautilusSpecialLocationBarClass))

typedef struct NautilusSpecialLocationBarPrivate NautilusSpecialLocationBarPrivate;

typedef struct
{
	GtkInfoBar parent;

	NautilusSpecialLocationBarPrivate *priv;
} NautilusSpecialLocationBar;

typedef enum {
	NAUTILUS_SPECIAL_LOCATION_TEMPLATES,
	NAUTILUS_SPECIAL_LOCATION_SCRIPTS,
} NautilusSpecialLocation;

typedef struct
{
	GtkInfoBarClass parent_class;
} NautilusSpecialLocationBarClass;

GType		 nautilus_special_location_bar_get_type	(void) G_GNUC_CONST;

GtkWidget	*nautilus_special_location_bar_new (NautilusSpecialLocation location);

G_END_DECLS

#endif /* __NAUTILUS_SPECIAL_LOCATION_BAR_H */
