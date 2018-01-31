/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nemo-job-queue.c - file operation queue

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

#include <config.h>

#include "nemo-job-queue.h"
#include "nemo-global-preferences.h"

struct _NemoJobQueuePriv {
	GList *queued_jobs;
    GList *running_jobs;
    gulong pref_changed_id;
    gboolean skip_queue;
};

enum {
	NEW_JOB,
	LAST_SIGNAL
};

typedef struct {
    GIOSchedulerJobFunc job_func;
    gpointer user_data;
    NemoProgressInfo *info;
    GCancellable *cancellable;
} Job;

static NemoJobQueue *singleton = NULL;

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (NemoJobQueue, nemo_job_queue,
               G_TYPE_OBJECT);

static void
nemo_job_queue_finalize (GObject *obj)
{
	NemoJobQueue *self = NEMO_JOB_QUEUE (obj);

	if (self->priv->queued_jobs != NULL) {
		g_list_free_full (self->priv->queued_jobs, g_object_unref);
	}

    if (self->priv->pref_changed_id != 0) {
        g_signal_handler_disconnect (nemo_preferences, self->priv->pref_changed_id);
        self->priv->pref_changed_id = 0;
    }

	G_OBJECT_CLASS (nemo_job_queue_parent_class)->finalize (obj);
}

static GObject *
nemo_job_queue_constructor (GType type,
					    guint n_props,
					    GObjectConstructParam *props)
{
	GObject *retval;

	if (singleton != NULL) {
		return g_object_ref (singleton);
	}

	retval = G_OBJECT_CLASS (nemo_job_queue_parent_class)->constructor
		(type, n_props, props);

	singleton = NEMO_JOB_QUEUE (retval);
	g_object_add_weak_pointer (retval, (gpointer) &singleton);

	return retval;
}

static void
pref_changed_cb (NemoJobQueue *self)
{
    self->priv->skip_queue = g_settings_get_boolean (nemo_preferences,
                                                     NEMO_PREFERENCES_NEVER_QUEUE_FILE_OPS);
}

static void
nemo_job_queue_init (NemoJobQueue *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NEMO_TYPE_JOB_QUEUE,
						  NemoJobQueuePriv);

    self->priv->queued_jobs = NULL;
    self->priv->running_jobs = NULL;

    self->priv->pref_changed_id = g_signal_connect_swapped (nemo_preferences,
                                                    "changed::" NEMO_PREFERENCES_NEVER_QUEUE_FILE_OPS,
                                                    G_CALLBACK (pref_changed_cb), self);

    pref_changed_cb (self);
}

static void
nemo_job_queue_class_init (NemoJobQueueClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->constructor = nemo_job_queue_constructor;
	oclass->finalize = nemo_job_queue_finalize;

	signals[NEW_JOB] =
		g_signal_new ("new-job",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE,
			      0,
			      NULL);

	g_type_class_add_private (klass, sizeof (NemoJobQueuePriv));
}

static gint
compare_info_func (gconstpointer a,
                   gconstpointer b)
{
    Job *job = (Job*) a;
    return (job->info == b) ? 0 : 1;
}

static gint
compare_job_data_func (gconstpointer a,
                       gconstpointer b)
{
    Job *job = (Job*) a;
    return (job->job_func == b) ? 0 : 1;
}

static void
job_finished_cb (NemoJobQueue *self,
                 NemoProgressInfo *info)
{
    GList *ptr;

    ptr = g_list_find_custom (self->priv->running_jobs, info, (GCompareFunc) compare_info_func);
    if (!ptr)
        ptr = g_list_find_custom (self->priv->queued_jobs, info, (GCompareFunc) compare_info_func);

    Job *job = ptr->data;

    self->priv->running_jobs = g_list_remove (self->priv->running_jobs, job);
    self->priv->queued_jobs = g_list_remove (self->priv->queued_jobs, job);

    g_slice_free (Job, job);

    nemo_job_queue_start_next_job (self);
}

static void start_job (NemoJobQueue *self, Job *job);

NemoJobQueue *
nemo_job_queue_get (void)
{
    return g_object_new (NEMO_TYPE_JOB_QUEUE, NULL);
}

void
nemo_job_queue_add_new_job (NemoJobQueue         *self,
                            GIOSchedulerJobFunc   job_func,
                            gpointer              user_data,
                            GCancellable         *cancellable,
                            NemoProgressInfo     *info,
                            gboolean              start_immediately)
{
	if (g_list_find_custom (self->priv->queued_jobs, user_data, (GCompareFunc) compare_job_data_func) != NULL) {
		g_warning ("Adding the same file job object to the job queue");
		return;
	}

    Job *new_job = g_slice_new0 (Job);
    new_job->job_func = job_func;
    new_job->user_data = user_data;
    new_job->cancellable = cancellable;
    new_job->info = info;

	self->priv->queued_jobs =
		g_list_append (self->priv->queued_jobs, new_job);

    nemo_progress_info_queue (info);

	g_signal_connect_swapped (info, "finished",
                              G_CALLBACK (job_finished_cb), self);

    if (self->priv->skip_queue || start_immediately)
        start_job (self, new_job);
    else
        nemo_job_queue_start_next_job (self);

    g_signal_emit (self, signals[NEW_JOB], 0, NULL);
}

static void
start_job (NemoJobQueue *self, Job *job)
{
    self->priv->queued_jobs = g_list_remove (self->priv->queued_jobs, job);

    g_io_scheduler_push_job (job->job_func,
                             job->user_data,
                             NULL, // destroy notify
                             0,
                             job->cancellable);

    self->priv->running_jobs = g_list_append (self->priv->running_jobs, job);
}

void
nemo_job_queue_start_next_job (NemoJobQueue *self)
{
    if (g_list_length (self->priv->running_jobs) == 0 && g_list_length (self->priv->queued_jobs) > 0)
        start_job (self, self->priv->queued_jobs->data);
}

void
nemo_job_queue_start_job_by_info (NemoJobQueue     *self,
                                  NemoProgressInfo *info)
{
    GList *target = g_list_find_custom (self->priv->queued_jobs, info, (GCompareFunc) compare_info_func);

    if (target)
        start_job (self, target->data);
}

GList *
nemo_job_queue_get_all_jobs (NemoJobQueue *self)
{
	return self->priv->queued_jobs;
}
