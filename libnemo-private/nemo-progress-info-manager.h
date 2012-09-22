/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nemo
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NEMO_PROGRESS_INFO_MANAGER_H__
#define __NEMO_PROGRESS_INFO_MANAGER_H__

#include <glib-object.h>

#include <libnemo-private/nemo-progress-info.h>

#define NEMO_TYPE_PROGRESS_INFO_MANAGER nemo_progress_info_manager_get_type()
#define NEMO_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROGRESS_INFO_MANAGER, NemoProgressInfoManager))
#define NEMO_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PROGRESS_INFO_MANAGER, NemoProgressInfoManagerClass))
#define NEMO_IS_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROGRESS_INFO_MANAGER))
#define NEMO_IS_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PROGRESS_INFO_MANAGER))
#define NEMO_PROGRESS_INFO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PROGRESS_INFO_MANAGER, NemoProgressInfoManagerClass))

typedef struct _NemoProgressInfoManager NemoProgressInfoManager;
typedef struct _NemoProgressInfoManagerClass NemoProgressInfoManagerClass;
typedef struct _NemoProgressInfoManagerPriv NemoProgressInfoManagerPriv;

struct _NemoProgressInfoManager {
  GObject parent;

  /* private */
  NemoProgressInfoManagerPriv *priv;
};

struct _NemoProgressInfoManagerClass {
  GObjectClass parent_class;
};

GType nemo_progress_info_manager_get_type (void);

NemoProgressInfoManager* nemo_progress_info_manager_new (void);

void nemo_progress_info_manager_add_new_info (NemoProgressInfoManager *self,
                                                  NemoProgressInfo *info);
GList *nemo_progress_info_manager_get_all_infos (NemoProgressInfoManager *self);

G_END_DECLS

#endif /* __NEMO_PROGRESS_INFO_MANAGER_H__ */
