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
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NEMO_PROGRESS_INFO_H
#define NEMO_PROGRESS_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

#define NEMO_TYPE_PROGRESS_INFO         (nemo_progress_info_get_type ())
#define NEMO_PROGRESS_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_PROGRESS_INFO, NemoProgressInfo))
#define NEMO_PROGRESS_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_PROGRESS_INFO, NemoProgressInfoClass))
#define NEMO_IS_PROGRESS_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_PROGRESS_INFO))
#define NEMO_IS_PROGRESS_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_PROGRESS_INFO))
#define NEMO_PROGRESS_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_PROGRESS_INFO, NemoProgressInfoClass))

typedef struct _NemoProgressInfo      NemoProgressInfo;
typedef struct _NemoProgressInfoClass NemoProgressInfoClass;

GType nemo_progress_info_get_type (void) G_GNUC_CONST;

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done
   
   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

NemoProgressInfo *nemo_progress_info_new (void);

GList *       nemo_get_all_progress_info (void);

char *        nemo_progress_info_get_status      (NemoProgressInfo *info);
char *        nemo_progress_info_get_details     (NemoProgressInfo *info);
double        nemo_progress_info_get_progress    (NemoProgressInfo *info);
GCancellable *nemo_progress_info_get_cancellable (NemoProgressInfo *info);
void          nemo_progress_info_cancel          (NemoProgressInfo *info);
gboolean      nemo_progress_info_get_is_started  (NemoProgressInfo *info);
gboolean      nemo_progress_info_get_is_finished (NemoProgressInfo *info);
gboolean      nemo_progress_info_get_is_paused   (NemoProgressInfo *info);

void          nemo_progress_info_start           (NemoProgressInfo *info);
void          nemo_progress_info_finish          (NemoProgressInfo *info);
void          nemo_progress_info_pause           (NemoProgressInfo *info);
void          nemo_progress_info_resume          (NemoProgressInfo *info);
void          nemo_progress_info_set_status      (NemoProgressInfo *info,
						      const char           *status);
void          nemo_progress_info_take_status     (NemoProgressInfo *info,
						      char                 *status);
void          nemo_progress_info_set_details     (NemoProgressInfo *info,
						      const char           *details);
void          nemo_progress_info_take_details    (NemoProgressInfo *info,
						      char                 *details);
void          nemo_progress_info_set_progress    (NemoProgressInfo *info,
						      double                current,
						      double                total);
void          nemo_progress_info_pulse_progress  (NemoProgressInfo *info);



#endif /* NEMO_PROGRESS_INFO_H */
