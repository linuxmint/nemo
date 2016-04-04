/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Daniel Schürmann <daschuer@gmx.de>
 */

/* nemo-background-window.h
 */

#ifndef NEMO_BACKGROUND_WINDOW_H
#define NEMO_BACKGROUND_WINDOW_H

#include "nemo-window.h"

#define NEMO_TYPE_BACKGROUND_WINDOW nemo_background_window_get_type()
#define NEMO_BACKGROUND_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_BACKGROUND_WINDOW, NemoBackgroundWindow))
#define NEMO_BACKGROUND_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_BACKGROUND_WINDOW, NemoBackgroundWindowClass))
#define NEMO_IS_BACKGROUND_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_BACKGROUND_WINDOW))
#define NEMO_IS_BACKGROUND_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_BACKGROUND_WINDOW))
#define NEMO_BACKGROUND_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_BACKGROUND_WINDOW, NemoBackgroundWindowClass))

typedef struct NemoBackgroundWindowDetails NemoBackgroundWindowDetails;

typedef struct {
	NemoWindow parent_spot;
	NemoBackgroundWindowDetails *details;
	gboolean affect_background_on_next_location_change;
} NemoBackgroundWindow;

typedef struct {
	NemoWindowClass parent_spot;
} NemoBackgroundWindowClass;

GType nemo_background_window_get_type (void);
void nemo_background_window_update_directory (NemoBackgroundWindow *window);
gboolean nemo_background_window_loaded (NemoBackgroundWindow *window);
void nemo_background_window_ensure (void);
GtkWidget *nemo_background_window_get (void);

#endif /* NEMO_BACKGROUND_WINDOW_H */
