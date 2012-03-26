/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Nautilus navigation state
 *
 * Copyright (C) 2011 Red Hat Inc.
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
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_NAVIGATION_STATE_H__
#define __NAUTILUS_NAVIGATION_STATE_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define NAUTILUS_TYPE_NAVIGATION_STATE nautilus_navigation_state_get_type()
#define NAUTILUS_NAVIGATION_STATE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_NAVIGATION_STATE, NautilusNavigationState))
#define NAUTILUS_NAVIGATION_STATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_NAVIGATION_STATE, NautilusNavigationStateClass))
#define NAUTILUS_IS_NAVIGATION_STATE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_NAVIGATION_STATE))
#define NAUTILUS_IS_NAVIGATION_STATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_NAVIGATION_STATE))
#define NAUTILUS_NAVIGATION_STATE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_NAVIGATION_STATE, NautilusNavigationStateClass))

typedef struct _NautilusNavigationState NautilusNavigationState;
typedef struct _NautilusNavigationStateClass NautilusNavigationStateClass;
typedef struct _NautilusNavigationStateDetails NautilusNavigationStateDetails;

struct _NautilusNavigationState {
	GObject parent;
	NautilusNavigationStateDetails *priv;
};

struct _NautilusNavigationStateClass {
	GObjectClass parent_class;
};

/* GObject */
GType       nautilus_navigation_state_get_type  (void);

NautilusNavigationState * nautilus_navigation_state_new (GtkActionGroup *slave,
                                                         const gchar **action_names);

void nautilus_navigation_state_add_group (NautilusNavigationState *state,
                                          GtkActionGroup *group);
void nautilus_navigation_state_set_master (NautilusNavigationState *state,
                                           GtkActionGroup *master);
GtkActionGroup * nautilus_navigation_state_get_master (NautilusNavigationState *self);

void nautilus_navigation_state_sync_all (NautilusNavigationState *state);

void nautilus_navigation_state_set_boolean (NautilusNavigationState *self,
					    const gchar *action_name,
					    gboolean value);

#endif /* __NAUTILUS_NAVIGATION_STATE_H__ */
