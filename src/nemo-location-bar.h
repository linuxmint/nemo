/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

/* nemo-location-bar.h - Location bar for Nemo
 */

#ifndef NEMO_LOCATION_BAR_H
#define NEMO_LOCATION_BAR_H

#include <libnemo-private/nemo-entry.h>
#include <gtk/gtk.h>

#define NEMO_TYPE_LOCATION_BAR nemo_location_bar_get_type()
#define NEMO_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_LOCATION_BAR, NemoLocationBar))
#define NEMO_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_LOCATION_BAR, NemoLocationBarClass))
#define NEMO_IS_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_LOCATION_BAR))
#define NEMO_IS_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_LOCATION_BAR))
#define NEMO_LOCATION_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_LOCATION_BAR, NemoLocationBarClass))

typedef struct NemoLocationBarDetails NemoLocationBarDetails;

typedef struct NemoLocationBar {
	GtkBox parent;
	NemoLocationBarDetails *details;
} NemoLocationBar;

typedef struct {
	GtkBoxClass parent_class;

	/* for GtkBindingSet */
	void         (* cancel)           (NemoLocationBar *bar);
} NemoLocationBarClass;

GType      nemo_location_bar_get_type     	(void);
GtkWidget* nemo_location_bar_new          	(void);
NemoEntry * nemo_location_bar_get_entry (NemoLocationBar *location_bar);

void	nemo_location_bar_activate	 (NemoLocationBar *bar);
void    nemo_location_bar_set_location     (NemoLocationBar *bar,
						const char          *location);

#endif /* NEMO_LOCATION_BAR_H */
