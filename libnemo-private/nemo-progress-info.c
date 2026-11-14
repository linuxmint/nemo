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
#include <string.h>
#include <sys/stat.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <eel/eel-string.h>
#include <eel/eel-glib-extensions.h>
#include "nemo-progress-info.h"
#include "nemo-progress-info-manager.h"

enum {
  CHANGED,
  QUEUED,
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
    GCond *cond;
    GMutex info_lock;
    GTimer *time;

	char *status;
	char *details;
    char *initial_details;
	double progress;
	double progress_total; /* total passed to the last set_progress */
	gint64 bytes_done;
	char *live_path;     /* destination file being written right now, if known */
	char *live_dir;      /* overwrite: directory holding GLib's temp file, path found by fd */
	gint64 live_base;    /* bytes of all earlier files of this operation */
	gint64 live_size;    /* last size seen by the worker, -1 if none */
	guint live_gen;      /* bumped whenever live_path changes */
	gboolean live_pending; /* a stat is queued or running */
	gboolean activity_mode;
	gboolean started;
	gboolean finished;
	gboolean paused;
    gboolean queued;
	
	GSource *idle_source;
	gboolean source_is_now;
	
	gboolean start_at_idle;
	gboolean finish_at_idle;
	gboolean changed_at_idle;
	gboolean progress_at_idle;
    gboolean queue_at_idle;
};

struct _NemoProgressInfoClass
{
	GObjectClass parent_class;
};

G_DEFINE_TYPE (NemoProgressInfo, nemo_progress_info, G_TYPE_OBJECT)

/* Overwriting goes through a ".goutputstream-XXXXXX" temp file in the
 * destination directory (renamed over the target at the end). Its name is
 * private to GLib, but the file is open in this process: find it among our
 * own descriptors. Concurrent jobs must not pick the same one. */
static GMutex claim_lock;
static GHashTable *claimed = NULL;

static char *
live_claim_temp (const char *dir)
{
	g_autofree char *prefix = g_strconcat (dir, "/.goutputstream-", NULL);
	GDir *fds = g_dir_open ("/proc/self/fd", 0, NULL);
	const char *name;
	char *found = NULL;

	if (fds == NULL) {
		return NULL;
	}
	g_mutex_lock (&claim_lock);
	if (claimed == NULL) {
		claimed = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	}
	while (found == NULL && (name = g_dir_read_name (fds)) != NULL) {
		g_autofree char *link = g_build_filename ("/proc/self/fd", name, NULL);
		g_autofree char *target = g_file_read_link (link, NULL);

		if (target != NULL &&
		    g_str_has_prefix (target, prefix) &&
		    strchr (target + strlen (prefix), '/') == NULL &&
		    !g_hash_table_contains (claimed, target)) {
			found = g_strdup (target);
			g_hash_table_add (claimed, g_strdup (target));
		}
	}
	g_mutex_unlock (&claim_lock);
	g_dir_close (fds);
	return found;
}

static void
live_unclaim_temp (const char *path)
{
	g_mutex_lock (&claim_lock);
	if (claimed != NULL) {
		g_hash_table_remove (claimed, path);
	}
	g_mutex_unlock (&claim_lock);
}

/* info_lock held */
static void
live_release_claim (NemoProgressInfo *info)
{
	if (info->live_dir != NULL && info->live_path != NULL) {
		live_unclaim_temp (info->live_path);
	}
}

static void
nemo_progress_info_finalize (GObject *object)
{
	NemoProgressInfo *info;
	
	info = NEMO_PROGRESS_INFO (object);

	g_free (info->status);
	g_free (info->details);
	live_release_claim (info);
	g_free (info->live_path);
	g_free (info->live_dir);
    g_free (info->initial_details);
	g_object_unref (info->cancellable);
    g_timer_destroy (info->time);
	
	if (G_OBJECT_CLASS (nemo_progress_info_parent_class)->finalize) {
		(*G_OBJECT_CLASS (nemo_progress_info_parent_class)->finalize) (object);
	}
}

static void
nemo_progress_info_dispose (GObject *object)
{
	NemoProgressInfo *info;
	
	info = NEMO_PROGRESS_INFO (object);

	g_mutex_lock (&info->info_lock);

	/* Destroy source in dispose, because the callback
	   could come here before the destroy, which should
	   ressurect the object for a while */
	if (info->idle_source) {
		g_source_destroy (info->idle_source);
		g_source_unref (info->idle_source);
		info->idle_source = NULL;
	}
	g_mutex_unlock (&info->info_lock);
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
	
    signals[QUEUED] =
        g_signal_new ("queued",
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
    info->cond = g_cond_new ();
    info->time = g_timer_new ();
    g_mutex_init (&info->info_lock);
    info->live_size = -1;

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
	
	g_mutex_lock (&info->info_lock);
	
	if (info->status) {
		res = g_strdup (info->status);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	g_mutex_unlock (&info->info_lock);
	
	return res;
}

char *
nemo_progress_info_get_details (NemoProgressInfo *info)
{
	char *res;
	
	g_mutex_lock (&info->info_lock);
	
	if (info->details) {
		res = g_strdup (info->details);
	} else {
		res = g_strdup (_("Preparing"));
	}
	
	g_mutex_unlock (&info->info_lock);

	return res;
}

char *
nemo_progress_info_get_initial_details (NemoProgressInfo *info)
{
    char *res;
    
    g_mutex_lock (&info->info_lock);
    
    if (info->initial_details) {
        res = g_strdup (info->initial_details);
    } else {
        res = g_strdup (_("Preparing"));
    }
    
    g_mutex_unlock (&info->info_lock);

    return res;
}

double
nemo_progress_info_get_progress (NemoProgressInfo *info)
{
	double res;
	
	g_mutex_lock (&info->info_lock);

	if (info->activity_mode) {
		res = -1.0;
	} else {
		res = info->progress;
	}
	
	g_mutex_unlock (&info->info_lock);
	
	return res;
}

void
nemo_progress_info_cancel (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);
	
	g_cancellable_cancel (info->cancellable);

    info->paused = FALSE;
    g_cond_signal (info->cond);

	g_mutex_unlock (&info->info_lock);
}

GCancellable *
nemo_progress_info_get_cancellable (NemoProgressInfo *info)
{
	GCancellable *c;
	
	g_mutex_lock (&info->info_lock);
	
	c = g_object_ref (info->cancellable);
	
	g_mutex_unlock (&info->info_lock);
	
	return c;
}

gboolean
nemo_progress_info_get_is_started (NemoProgressInfo *info)
{
	gboolean res;
	
	g_mutex_lock (&info->info_lock);
	
	res = info->started;
	
	g_mutex_unlock (&info->info_lock);
	
	return res;
}

gboolean
nemo_progress_info_get_is_finished (NemoProgressInfo *info)
{
	gboolean res;
	
	g_mutex_lock (&info->info_lock);
	
	res = info->finished;
	
	g_mutex_unlock (&info->info_lock);
	
	return res;
}

gboolean
nemo_progress_info_get_is_paused (NemoProgressInfo *info)
{
	gboolean res;
	
	g_mutex_lock (&info->info_lock);
	
	res = info->paused;
	
	g_mutex_unlock (&info->info_lock);
	
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
    gboolean queue_at_idle;
	GSource *source;

	source = g_main_current_source ();
	
	g_mutex_lock (&info->info_lock);

	/* Protect agains races where the source has
	   been destroyed on another thread while it
	   was being dispatched.
	   Similar to what gdk_threads_add_idle does.
	*/
	if (g_source_is_destroyed (source)) {
		g_mutex_unlock (&info->info_lock);
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
    queue_at_idle = info->queue_at_idle;
	
	info->start_at_idle = FALSE;
	info->finish_at_idle = FALSE;
	info->changed_at_idle = FALSE;
	info->progress_at_idle = FALSE;
    info->queue_at_idle = FALSE;
	
	g_mutex_unlock (&info->info_lock);

    if (queue_at_idle) {
        g_signal_emit (info,
                   signals[QUEUED],
                   0);
    }

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
nemo_progress_info_queue (NemoProgressInfo *info)
{
    g_mutex_lock (&info->info_lock);
    
    if (!info->queued) {
        info->queued = TRUE;
        
        info->queue_at_idle = TRUE;
        queue_idle (info, TRUE);
    }
    
    g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_pause (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);

	if (!info->paused) {
		info->paused = TRUE;
        g_timer_stop (info->time);
	}

	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_resume (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);

	if (info->paused) {
		info->paused = FALSE;
        if (!g_timer_is_active (info->time)) {
            g_timer_continue (info->time);
        }
	}

    g_cond_signal (info->cond);

	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_start (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);
	
	if (!info->started) {
		info->started = TRUE;
		
		info->start_at_idle = TRUE;
		queue_idle (info, TRUE);
	}

    g_timer_start (info->time);

	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_finish (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);
	
	if (!info->finished) {
		info->finished = TRUE;
		
		info->finish_at_idle = TRUE;
		queue_idle (info, TRUE);
	}
	
	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_take_status (NemoProgressInfo *info,
				    char *status)
{
	g_mutex_lock (&info->info_lock);
	
	if (g_strcmp0 (info->status, status) != 0) {
		g_free (info->status);
		info->status = status;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (status);
	}
	
	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_set_status (NemoProgressInfo *info,
				   const char *status)
{
	g_mutex_lock (&info->info_lock);
	
	if (g_strcmp0 (info->status, status) != 0) {
		g_free (info->status);
		info->status = g_strdup (status);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
	
	g_mutex_unlock (&info->info_lock);
}


void
nemo_progress_info_take_details (NemoProgressInfo *info,
				     char           *details)
{
	g_mutex_lock (&info->info_lock);
	
	if (g_strcmp0 (info->details, details) != 0) {
		g_free (info->details);
		info->details = details;
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	} else {
		g_free (details);
	}
  
	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_set_details (NemoProgressInfo *info,
				    const char           *details)
{
	g_mutex_lock (&info->info_lock);
	
	if (g_strcmp0 (info->details, details) != 0) {
		g_free (info->details);
		info->details = g_strdup (details);
		
		info->changed_at_idle = TRUE;
		queue_idle (info, FALSE);
	}
  
	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_take_initial_details (NemoProgressInfo *info,
                                                     char *initial_details)
{
    g_mutex_lock (&info->info_lock);
    
    if (g_strcmp0 (info->initial_details, initial_details) != 0) {
        g_free (info->initial_details);
        info->initial_details = initial_details;
        
        info->changed_at_idle = TRUE;
        queue_idle (info, FALSE);
    } else {
        g_free (initial_details);
    }
  
    g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_pulse_progress (NemoProgressInfo *info)
{
	g_mutex_lock (&info->info_lock);

	info->activity_mode = TRUE;
	info->progress = 0.0;
	info->progress_at_idle = TRUE;
	queue_idle (info, FALSE);
	

    while (info->paused) {
        g_cond_wait (info->cond, &info->info_lock);
    }

	g_mutex_unlock (&info->info_lock);
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
	
	g_mutex_lock (&info->info_lock);
	
	if (info->activity_mode || /* emit on switch from activity mode */
	    fabs (current_percent - info->progress) > 0
	    ) {
		info->activity_mode = FALSE;
		info->progress = current_percent;
		info->progress_total = total;
		info->progress_at_idle = TRUE;
		queue_idle (info, FALSE);
	}

    while (info->paused) {
        g_cond_wait (info->cond, &info->info_lock);
    }

	g_mutex_unlock (&info->info_lock);

}

void
nemo_progress_info_set_bytes_done (NemoProgressInfo *info,
                                   gint64            bytes)
{
	g_mutex_lock (&info->info_lock);
	info->bytes_done = bytes;
	g_mutex_unlock (&info->info_lock);
}

gint64
nemo_progress_info_get_bytes_done (NemoProgressInfo *info)
{
	gint64 res;

	g_mutex_lock (&info->info_lock);
	res = info->bytes_done;
	g_mutex_unlock (&info->info_lock);

	return res;
}

/* stat() can block for a long time on a stalled network or FUSE mount (gvfs
 * paths are local-looking), so it must never run in the main loop: a small
 * shared pool does it, and the main loop only reads the last result. */
typedef struct {
	NemoProgressInfo *info;
	char *path;   /* file to stat, or NULL while it is still to be found */
	char *dir;    /* overwrite: where to look for the temp file */
	guint gen;
} LiveStatRequest;

static void
live_stat_worker (gpointer data, gpointer user_data)
{
	LiveStatRequest *req = data;
	GStatBuf st;
	gint64 size = -1;
	char *found = NULL;
	const char *path = req->path;

	if (path == NULL && req->dir != NULL) {
		found = live_claim_temp (req->dir);
		path = found;
	}

	/* lstat: a symlink at the destination is not a file being written */
	if (path != NULL && g_lstat (path, &st) == 0 && S_ISREG (st.st_mode)) {
		size = (gint64) st.st_size;
	}

	g_mutex_lock (&req->info->info_lock);
	if (req->gen == req->info->live_gen) {
		req->info->live_size = size;
		if (found != NULL && size >= 0) {
			/* remember it for the next samples; the claim goes with it */
			req->info->live_path = found;
			found = NULL;
		} else if (req->dir != NULL && req->path != NULL && size < 0) {
			/* the temp file is gone (renamed at the end): look again */
			live_release_claim (req->info);
			g_clear_pointer (&req->info->live_path, g_free);
		}
	}
	req->info->live_pending = FALSE;
	g_mutex_unlock (&req->info->info_lock);

	if (found != NULL) {
		live_unclaim_temp (found);
		g_free (found);
	}
	g_object_unref (req->info);
	g_free (req->path);
	g_free (req->dir);
	g_free (req);
}

static GThreadPool *
live_stat_pool (void)
{
	static GThreadPool *pool = NULL;

	if (g_once_init_enter (&pool)) {
		g_once_init_leave (&pool, g_thread_pool_new (live_stat_worker, NULL, 2, FALSE, NULL));
	}
	return pool;
}

void
nemo_progress_info_set_live_file (NemoProgressInfo *info,
                                  const char       *path,
                                  gint64            base_bytes)
{
	g_mutex_lock (&info->info_lock);
	live_release_claim (info);
	g_free (info->live_path);
	g_clear_pointer (&info->live_dir, g_free);
	info->live_path = g_strdup (path);
	info->live_base = base_bytes;
	info->live_size = -1;
	info->live_gen++;
	g_mutex_unlock (&info->info_lock);
}

void
nemo_progress_info_set_live_temp (NemoProgressInfo *info,
                                  const char       *dir,
                                  gint64            base_bytes)
{
	g_mutex_lock (&info->info_lock);
	live_release_claim (info);
	g_clear_pointer (&info->live_path, g_free);
	g_free (info->live_dir);
	info->live_dir = g_strdup (dir);
	info->live_base = base_bytes;
	info->live_size = -1;
	info->live_gen++;
	g_mutex_unlock (&info->info_lock);
}

gint64
nemo_progress_info_get_live_bytes (NemoProgressInfo *info)
{
	LiveStatRequest *req = NULL;
	gint64 res = -1;

	g_mutex_lock (&info->info_lock);
	if (info->live_path != NULL || info->live_dir != NULL) {
		if (info->live_size >= 0) {
			res = info->live_base + info->live_size;
		}
		if (!info->live_pending) {
			info->live_pending = TRUE;
			req = g_new0 (LiveStatRequest, 1);
			req->info = g_object_ref (info);
			req->path = g_strdup (info->live_path);
			req->dir = g_strdup (info->live_dir);
			req->gen = info->live_gen;
		}
	}
	g_mutex_unlock (&info->info_lock);

	if (req != NULL) {
		g_thread_pool_push (live_stat_pool (), req, NULL);
	}

	return res;
}

double
nemo_progress_info_get_smooth_progress (NemoProgressInfo *info)
{
	double res, total;
	gint64 live;

	/* also kicks off the next async stat of the growing destination file */
	live = nemo_progress_info_get_live_bytes (info);

	g_mutex_lock (&info->info_lock);
	res = info->activity_mode ? -1.0 : info->progress;
	total = info->progress_total;
	g_mutex_unlock (&info->info_lock);

	if (res >= 0 && live >= 0 && total > 0) {
		res = MAX (res, MIN ((double) live / total, 1.0));
	}

	return res;
}

gdouble
nemo_progress_info_get_elapsed_time (NemoProgressInfo *info)
{
    gdouble elapsed;

    g_mutex_lock (&info->info_lock);
    elapsed = g_timer_elapsed (info->time, NULL);
    g_mutex_unlock (&info->info_lock);

    return elapsed;
}