/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-progress-info.h: file operation progress info.
 
   Copyright (C) 2007 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include <math.h>
#include <glib/gi18n.h>
#include <eel/eel-string.h>
#include <eel/eel-glib-extensions.h>
#include "nemo-progress-info.h"
#include "nemo-progress-info-manager.h"
#include "nemo-icon-info.h"

enum {
  CHANGED,
  PROGRESS_CHANGED,
  STARTED,
  FINISHED,
  LAST_SIGNAL
};

#define SIGNAL_DELAY_MSEC 100

static guint signals[LAST_SIGNAL] = { 0 };

struct _NemoProgressInfo
{
	GObject parent_instance;
	
	GCancellable *cancellable;
	
	char *status;
	char *details;
	double progress;
	gboolean activity_mode;
	gboolean started;
	gboolean finished;
	gboolean paused;
	
	GSource *idle_source;
	gboolean source_is_now;
	
	gboolean start_at_idle;
	gboolean finish_at_idle;
	gboolean changed_at_idle;
	gboolean progress_at_idle;
};

struct _NemoProgressInfoClass
{
	GObjectClass parent_class;
};

G_LOCK_DEFINE_STATIC(progress_info);

G_DEFINE_TYPE (NemoProgressInfo, nemo_progress_info, G_TYPE_OBJECT)

static void
nemo_progress_info_finalize (GObject *object)
{
	NemoProgressInfo *info;
	
	info = NEMO_PROGRESS_INFO (object);

	g_free (info->status);
	g_free (info->details);
	g_object_unref (info->cancellable);
	
	if (G_OBJECT_CLASS (nemo_progress_info_parent_class)->finalize) {
		(*G_OBJECT_CLASS (nemo_progress_info_parent_class)->finalize) (object);
	}
}

static void
nemo_progress_info_dispose (GObject *object)
{
	NemoProgressInfo *info;
	
	info = NEMO_PROGRESS_INFO (object);

	G_LOCK (progress_info);

	/* Destroy source in dispose, because the callback
	   could come here before the destroy, which should
	   ressurect the object for a while */
	if (info->idle_source) {
		g_source_destroy (info->idle_source);
		g_source_unref (info->idle_source);
		info->idle_source = NULL;
	}
	G_UNLOCK (progress_info);
}

static void
nemo_progress_info_class_init (NemoProgressInfoClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	
	gobject_class->finalize = nemo_progress_info_finalize;
	gobject_class->dispose = nemo_progress_info_dispose;
	
	signals[CHANGED] =
		g_signal_new ("changed",
			      NEMO_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[PROGRESS_CHANGED] =
		g_signal_new ("progress-changed",
			      NEMO_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[STARTED] =
		g_signal_new ("started",
			      NEMO_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
	signals[FINISHED] =
		g_signal_new ("finished",
			      NEMO_TYPE_PROGRESS_INFO,
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	
}

static void
nemo_progress_info_init (NemoProgressInfo *info)
{
	NemoProgressInfoManager *manager;

	info->cancellable = g_cancellable_new ();

	manager = nemo_progress_info_manager_new ();
	nemo_progress_info_manager_add_new_info (manager, info);
	g_object_unref (manager);
}

NemoProgressInfo *
nemo_progress_info_new (void)
{
	NemoProgressInfo *info;
	
	info = g_object_new (NEMO_TYPE_PROGRESS_INFO, NULL);
	
	return info;
}

char *
nemo_progress_info_get_status (NemoProgressInfo *info)
{
	char *res;
	
	G_LOCK (progress_info);
	
	if (info->status) {
		res = g_strdup (info->status);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	G_UNLOCK (progress_info);
	
	return res;
}

char *
nemo_progress_info_get_details (NemoProgressInfo *info)
{
	char *res;
	
	G_LOCK (progress_info);
	
	if (info->details) {
		res = g_strdup (info->details);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	G_UNLOCK (progress_info);

	return res;
}

double
nemo_progress_info_get_progress (NemoProgressInfo *info)
{
	double res;
	
	G_LOCK (progress_info);

	if (info->activity_mode) {
		res = -1.0;
	} else {
		res = info->progress;
	}
	
	G_UNLOCK (progress_info);
	
	return res;
}

void
nemo_progress_info_cancel (NemoProgressInfo *info)
{
	G_LOCK (progress_info);
	
	g_cancellable_cancel (info->cancellable);
	
	G_UNLOCK (progress_info);
}

GCancellable *
nemo_progress_info_get_cancellable (NemoProgressInfo *info)
{
	GCancellable *c;
	
	G_LOCK (progress_info);
	
	c = g_object_ref (info->cancellable);
	
	G_UNLOCK (progress_info);
	
	return c;
}

gboolean
nemo_progress_info_get_is_started (NemoProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->started;
	
	G_UNLOCK (progress_info);
	
	return res;
}

gboolean
nemo_progress_info_get_is_finished (NemoProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->finished;
	
	G_UNLOCK (progress_info);
	
	return res;
}

gboolean
nemo_progress_info_get_is_paused (NemoProgressInfo *info)
{
	gboolean res;
	
	G_LOCK (progress_info);
	
	res = info->paused;
	
	G_UNLOCK (progress_info);
	
	return res;
}

static gboolean
idle_callback (gpointer data)
{
	NemoProgressInfo *info = data;
	gboolean start_at_idle;
	gboolean finish_at_idle;
	gboolean changed_at_idle;
	gboolean progress_at_idle;
	GSource *source;

	source = g_main_current_source ();
	
	G_LOCK (progress_info);

	/* Protect agains races where the source has
	   been destroyed on another thread while it
	   was being dispatched.
	   Similar to what gdk_threads_add_idle does.
	*/
	if (g_source_is_destroyed (source)) {
		G_UNLOCK (progress_info);
		return FALSE;
	}

	/* We hadn't destroyed the source, so take a ref.
	 * This might ressurect the object from dispose, but
	 * that should be ok.
	 */
	g_object_ref (info);

	g_assert (source == info->idle_source);
	
	g_source_unref (source);
	info->idle_source = NULL;
	
	start_at_idle = info->start_at_idle;
	finish_at_idle = info->finish_at_idle;
	changed_at_idle = info->changed_at_idle;
	progress_at_idle = info->progress_at_idle;
	
	info->start_at_idle = FALSE;
	info->finish_at_idle = FALSE;
	info->changed_at_idle = FALSE;
	info->progress_at_idle = FALSE;
	
	G_UNLOCK (progress_info);
	
	if (start_at_idle) {
		g_signal_emit (info,
			       signals[STARTED],
			       0);
	}
	
	if (changed_at_idle) {
		g_signal_emit (info,
			       signals[CHANGED],
			       0);
	}
	
	if (progress_at_idle) {
		g_signal_emit (info,
			       signals[PROGRESS_CHANGED],
			       0);
	}
	
	if (finish_at_idle) {
		g_signal_emit (info,
			       signals[FINISHED],
			       0);
	}
	
	g_object_unref (info);
	
	return FALSE;
}

/* Called with lock held */
static void
queue_idle (NemoProgressInfo *info, gboolean now)
{
	if (info->idle_source == NULL ||
	    (now && !info->source_is_now)) {
		if (info->idle_source) {
			g_source_destroy (info->idle_source);
			g_source_unref (info->idle_source);
			info->idle_source = NULL;
		}
		
		info->source_is_now = now;
		if (now) {
			info->idle_source = g_idle_source_new ();
		} else {
			info->idle_source = g_timeout_source_new (SIGNAL_DELAY_MSEC);
		}
		g_source_set_callback (info->idle_source, idle_callback, info, NULL);
		g_source_attach (info->idle_source, NULL);
	}
}

void
nemo_progress_info_pause (NemoProgressInfo *info)
{
	G_LOCK (progress_info);

	if (!info->paused) {
		info->paused = TRUE;
	}

	G_UNLOCK (progress_info);
}

void
nemo_progress_info_resume (NemoProgressInfo *info)
{
	G_LOCK (progress_info);

	if (info->paused) {
		info->paused = FALSE;
	}

	G_UNLOCK (progress_info);
}

void
nemo_progress_info_start (NemoProgressInfo *info)
{
	G_LOCK (progress_info);
	
	if (!info->started) {
		info->started = TRUE;
		
		info->start_at_idle = TRUE;
		queue_idle (info, TRUE);
	}
	
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_finish (NemoProgressInfo *info)
{
	G_LOCK (progress_info);
	
	if (!info->finished) {
		info->finished = TRUE;
		
		info->finish_at_idle = TRUE;
		queue_idle (info, TRUE);
	}
	
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_take_status (NemoProgressInfo *info,
				    char *status)
{
	G_LOCK (progress_info);
	
	if (g_strcmp0 (info->status, status) != 0) {
		g_free (info->status);
		info->status = status;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (status);
	}
	
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_set_status (NemoProgressInfo *info,
				   const char *status)
{
	G_LOCK (progress_info);
	
	if (g_strcmp0 (info->status, status) != 0) {
		g_free (info->status);
		info->status = g_strdup (status);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
	
	G_UNLOCK (progress_info);
}


void
nemo_progress_info_take_details (NemoProgressInfo *info,
				     char           *details)
{
	G_LOCK (progress_info);
	
	if (g_strcmp0 (info->details, details) != 0) {
		g_free (info->details);
		info->details = details;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (details);
	}
  
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_set_details (NemoProgressInfo *info,
				    const char           *details)
{
	G_LOCK (progress_info);
	
	if (g_strcmp0 (info->details, details) != 0) {
		g_free (info->details);
		info->details = g_strdup (details);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
  
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_pulse_progress (NemoProgressInfo *info)
{
	G_LOCK (progress_info);

	info->activity_mode = TRUE;
	info->progress = 0.0;
	info->progress_at_idle = TRUE;
	queue_idle (info, FALSE);
	
	G_UNLOCK (progress_info);
}

void
nemo_progress_info_set_progress (NemoProgressInfo *info,
				     double                current,
				     double                total)
{
	double current_percent;
	
	if (total <= 0) {
		current_percent = 1.0;
	} else {
		current_percent = current / total;

		if (current_percent < 0) {
			current_percent	= 0;
		}
		
		if (current_percent > 1.0) {
			current_percent	= 1.0;
		}
	}
	
	G_LOCK (progress_info);
	
	if (info->activity_mode || /* emit on switch from activity mode */
	    fabs (current_percent - info->progress) > 0.005 /* Emit on change of 0.5 percent */
	    ) {
		info->activity_mode = FALSE;
		info->progress = current_percent;
		info->progress_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
	
	G_UNLOCK (progress_info);
}
