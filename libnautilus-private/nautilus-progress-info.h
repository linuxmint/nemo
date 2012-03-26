/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-progress-info.h: file operation progress info.
 
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

#ifndef NAUTILUS_PROGRESS_INFO_H
#define NAUTILUS_PROGRESS_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_PROGRESS_INFO         (nautilus_progress_info_get_type ())
#define NAUTILUS_PROGRESS_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NAUTILUS_TYPE_PROGRESS_INFO, NautilusProgressInfo))
#define NAUTILUS_PROGRESS_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_PROGRESS_INFO, NautilusProgressInfoClass))
#define NAUTILUS_IS_PROGRESS_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NAUTILUS_TYPE_PROGRESS_INFO))
#define NAUTILUS_IS_PROGRESS_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_PROGRESS_INFO))
#define NAUTILUS_PROGRESS_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_PROGRESS_INFO, NautilusProgressInfoClass))

typedef struct _NautilusProgressInfo      NautilusProgressInfo;
typedef struct _NautilusProgressInfoClass NautilusProgressInfoClass;

GType nautilus_progress_info_get_type (void) G_GNUC_CONST;

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done
   
   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

NautilusProgressInfo *nautilus_progress_info_new (void);

GList *       nautilus_get_all_progress_info (void);

char *        nautilus_progress_info_get_status      (NautilusProgressInfo *info);
char *        nautilus_progress_info_get_details     (NautilusProgressInfo *info);
double        nautilus_progress_info_get_progress    (NautilusProgressInfo *info);
GCancellable *nautilus_progress_info_get_cancellable (NautilusProgressInfo *info);
void          nautilus_progress_info_cancel          (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_started  (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_finished (NautilusProgressInfo *info);
gboolean      nautilus_progress_info_get_is_paused   (NautilusProgressInfo *info);

void          nautilus_progress_info_start           (NautilusProgressInfo *info);
void          nautilus_progress_info_finish          (NautilusProgressInfo *info);
void          nautilus_progress_info_pause           (NautilusProgressInfo *info);
void          nautilus_progress_info_resume          (NautilusProgressInfo *info);
void          nautilus_progress_info_set_status      (NautilusProgressInfo *info,
						      const char           *status);
void          nautilus_progress_info_take_status     (NautilusProgressInfo *info,
						      char                 *status);
void          nautilus_progress_info_set_details     (NautilusProgressInfo *info,
						      const char           *details);
void          nautilus_progress_info_take_details    (NautilusProgressInfo *info,
						      char                 *details);
void          nautilus_progress_info_set_progress    (NautilusProgressInfo *info,
						      double                current,
						      double                total);
void          nautilus_progress_info_pulse_progress  (NautilusProgressInfo *info);



#endif /* NAUTILUS_PROGRESS_INFO_H */
