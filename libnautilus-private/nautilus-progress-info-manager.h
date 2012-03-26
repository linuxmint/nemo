/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nautilus
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NAUTILUS_PROGRESS_INFO_MANAGER_H__
#define __NAUTILUS_PROGRESS_INFO_MANAGER_H__

#include <glib-object.h>

#include <libnautilus-private/nautilus-progress-info.h>

#define NAUTILUS_TYPE_PROGRESS_INFO_MANAGER nautilus_progress_info_manager_get_type()
#define NAUTILUS_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PROGRESS_INFO_MANAGER, NautilusProgressInfoManager))
#define NAUTILUS_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROGRESS_INFO_MANAGER, NautilusProgressInfoManagerClass))
#define NAUTILUS_IS_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PROGRESS_INFO_MANAGER))
#define NAUTILUS_IS_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PROGRESS_INFO_MANAGER))
#define NAUTILUS_PROGRESS_INFO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PROGRESS_INFO_MANAGER, NautilusProgressInfoManagerClass))

typedef struct _NautilusProgressInfoManager NautilusProgressInfoManager;
typedef struct _NautilusProgressInfoManagerClass NautilusProgressInfoManagerClass;
typedef struct _NautilusProgressInfoManagerPriv NautilusProgressInfoManagerPriv;

struct _NautilusProgressInfoManager {
  GObject parent;

  /* private */
  NautilusProgressInfoManagerPriv *priv;
};

struct _NautilusProgressInfoManagerClass {
  GObjectClass parent_class;
};

GType nautilus_progress_info_manager_get_type (void);

NautilusProgressInfoManager* nautilus_progress_info_manager_new (void);

void nautilus_progress_info_manager_add_new_info (NautilusProgressInfoManager *self,
                                                  NautilusProgressInfo *info);
GList *nautilus_progress_info_manager_get_all_infos (NautilusProgressInfoManager *self);

G_END_DECLS

#endif /* __NAUTILUS_PROGRESS_INFO_MANAGER_H__ */
