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

#ifndef __NEMO_JOB_QUEUE_H__
#define __NEMO_JOB_QUEUE_H__

#include <glib-object.h>

#include <libnemo-private/nemo-progress-info.h>

#define NEMO_TYPE_JOB_QUEUE nemo_job_queue_get_type()
#define NEMO_JOB_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_JOB_QUEUE, NemoJobQueue))
#define NEMO_JOB_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_JOB_QUEUE, NemoJobQueueClass))
#define NEMO_IS_JOB_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_JOB_QUEUE))
#define NEMO_IS_JOB_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_JOB_QUEUE))
#define NEMO_JOB_QUEUE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_JOB_QUEUE, NemoJobQueueClass))

typedef struct _NemoJobQueue NemoJobQueue;
typedef struct _NemoJobQueueClass NemoJobQueueClass;
typedef struct _NemoJobQueuePriv NemoJobQueuePriv;

struct _NemoJobQueue {
  GObject parent;

  /* private */
  NemoJobQueuePriv *priv;
};

struct _NemoJobQueueClass {
  GObjectClass parent_class;
};

GType nemo_job_queue_get_type (void);

NemoJobQueue *nemo_job_queue_get (void);

void nemo_job_queue_add_new_job (NemoJobQueue *self,
                                 GIOSchedulerJobFunc job_func,
                                 gpointer user_data,
                                 GCancellable *cancellable,
                                 NemoProgressInfo *info);

void nemo_job_queue_start_next_job (NemoJobQueue *self);

void nemo_job_queue_start_job_by_info (NemoJobQueue     *self,
                                       NemoProgressInfo *info);

GList *nemo_job_queue_get_all_jobs (NemoJobQueue *self);

G_END_DECLS

#endif /* __NEMO_JOB_QUEUE_H__ */
