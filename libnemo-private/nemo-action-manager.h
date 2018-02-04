/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#ifndef NEMO_ACTION_MANAGER_H
#define NEMO_ACTION_MANAGER_H

#include <glib.h>
#include "nemo-file.h"

#define NEMO_TYPE_ACTION_MANAGER nemo_action_manager_get_type()
#define NEMO_ACTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_ACTION_MANAGER, NemoActionManager))
#define NEMO_ACTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_ACTION_MANAGER, NemoActionManagerClass))
#define NEMO_IS_ACTION_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_ACTION_MANAGER))
#define NEMO_IS_ACTION_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_ACTION_MANAGER))
#define NEMO_ACTION_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_ACTION_MANAGER, NemoActionManagerClass))

typedef struct _NemoActionManager NemoActionManager;
typedef struct _NemoActionManagerClass NemoActionManagerClass;

struct _NemoActionManager {
    GObject parent;
    GList *actions;
    GList *actions_directory_list;
    gboolean action_list_dirty;
};

struct _NemoActionManagerClass {
    GObjectClass parent_class;
    void (* changed) (NemoActionManager *action_manager);
};

GType         nemo_action_manager_get_type             (void);
NemoActionManager   *nemo_action_manager_new           (void);
GList *       nemo_action_manager_list_actions (NemoActionManager *action_manager);
gchar *       nemo_action_manager_get_user_directory_path (void);

#endif /* NEMO_ACTION_MANAGER_H */
