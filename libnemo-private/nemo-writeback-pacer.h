/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
   nemo-writeback-pacer.h - keeps copies to slow destinations in step
   with the device

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

#ifndef __NEMO_WRITEBACK_PACER_H__
#define __NEMO_WRITEBACK_PACER_H__

#include <gio/gio.h>

typedef struct NemoWritebackPacer NemoWritebackPacer;

/* Returns NULL unless the directory is on a removable drive or a network
 * filesystem, or NEMO_FORCE_WRITEBACK_PACING is set. Waits stop when the
 * cancellable is cancelled. */
NemoWritebackPacer *nemo_writeback_pacer_new    (GFile               *directory,
                                                 GCancellable        *cancellable);

/* When a window fills, waits for the previous one to sync. A failed sync is
 * reported once per pacer, by this or by finish. */
gboolean            nemo_writeback_pacer_add    (NemoWritebackPacer  *pacer,
                                                 goffset              bytes_written,
                                                 GError             **error);

/* Waits for everything written to reach the device, unless cancelled, then
 * frees the pacer. Sync errors after a cancel aren't seen. */
gboolean            nemo_writeback_pacer_finish (NemoWritebackPacer  *pacer,
                                                 GError             **error);

#endif /* __NEMO_WRITEBACK_PACER_H__ */
