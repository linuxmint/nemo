/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nemo - Nemo navigation state
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

#ifndef __NEMO_NAVIGATION_STATE_H__
#define __NEMO_NAVIGATION_STATE_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define NEMO_TYPE_NAVIGATION_STATE nemo_navigation_state_get_type()
#define NEMO_NAVIGATION_STATE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_NAVIGATION_STATE, NemoNavigationState))
#define NEMO_NAVIGATION_STATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_NAVIGATION_STATE, NemoNavigationStateClass))
#define NEMO_IS_NAVIGATION_STATE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_NAVIGATION_STATE))
#define NEMO_IS_NAVIGATION_STATE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_NAVIGATION_STATE))
#define NEMO_NAVIGATION_STATE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_NAVIGATION_STATE, NemoNavigationStateClass))

typedef struct _NemoNavigationState NemoNavigationState;
typedef struct _NemoNavigationStateClass NemoNavigationStateClass;
typedef struct _NemoNavigationStateDetails NemoNavigationStateDetails;

struct _NemoNavigationState {
	GObject parent;
	NemoNavigationStateDetails *priv;
};

struct _NemoNavigationStateClass {
	GObjectClass parent_class;
};

/* GObject */
GType       nemo_navigation_state_get_type  (void);

NemoNavigationState * nemo_navigation_state_new (GtkActionGroup *slave,
                                                         const gchar **action_names);

void nemo_navigation_state_add_group (NemoNavigationState *state,
                                          GtkActionGroup *group);
void nemo_navigation_state_set_master (NemoNavigationState *state,
                                           GtkActionGroup *master);
GtkActionGroup * nemo_navigation_state_get_master (NemoNavigationState *self);

void nemo_navigation_state_sync_all (NemoNavigationState *state);

void nemo_navigation_state_set_boolean (NemoNavigationState *self,
					    const gchar *action_name,
					    gboolean value);

#endif /* __NEMO_NAVIGATION_STATE_H__ */
