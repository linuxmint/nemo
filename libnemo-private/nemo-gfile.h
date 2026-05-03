/* nemo-gfile.h
 *
 * Copyright (C) 2006-2023 Red Hat, Inc.
 * Copyright (C) 2026 Nemo Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifndef __NEMO_GFILE_H__
#define __NEMO_GFILE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

/* Target progress update frequency (1 update in 1 second) */
#define NEMO_G_FILE_TARGET_MAX_CHUNK_TIME_US 1000000
#define NEMO_G_FILE_MIN_BUFFER_SIZE 1024 * 1024

typedef void (*NemoGFileProgressCallback) (goffset current_num_bytes,
                                           goffset total_num_bytes,
                                           gpointer user_data);

gboolean
nemo_g_file_copy_synchronous (GFile *source,
                              GFile *destination,
                              GFileCopyFlags flags,
                              GCancellable *cancellable,
                              NemoGFileProgressCallback progress_callback,
                              gpointer progress_callback_data,
                              GError **error);

gboolean
nemo_g_file_move_synchronous (GFile *source,
                              GFile *destination,
                              GFileCopyFlags flags,
                              GCancellable *cancellable,
                              NemoGFileProgressCallback progress_callback,
                              gpointer progress_callback_data,
                              GError **error);

G_END_DECLS

#endif /* __NEMO_GFILE_H__ */