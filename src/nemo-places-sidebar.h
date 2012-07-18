/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nemo
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *
 */
#ifndef _NEMO_PLACES_SIDEBAR_H
#define _NEMO_PLACES_SIDEBAR_H

#include "nemo-window.h"

#include <gtk/gtk.h>

#define NEMO_PLACES_SIDEBAR_ID    "places"

#define NEMO_TYPE_PLACES_SIDEBAR nemo_places_sidebar_get_type()
#define NEMO_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PLACES_SIDEBAR, NemoPlacesSidebar))
#define NEMO_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PLACES_SIDEBAR, NemoPlacesSidebarClass))
#define NEMO_IS_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PLACES_SIDEBAR))
#define NEMO_IS_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PLACES_SIDEBAR))
#define NEMO_PLACES_SIDEBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PLACES_SIDEBAR, NemoPlacesSidebarClass))


GType nemo_places_sidebar_get_type (void);
GtkWidget * nemo_places_sidebar_new (NemoWindow *window);


#endif
