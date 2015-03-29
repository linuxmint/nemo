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
 * see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NEMO_KEYFILE_METADATA_H__
#define __NEMO_KEYFILE_METADATA_H__

#include <glib.h>

#include <libnemo-private/nemo-file.h>

void nemo_keyfile_metadata_set_string (NemoFile *file,
                                           const char *keyfile_filename,
                                           const gchar *name,
                                           const gchar *key,
                                           const gchar *string);

void nemo_keyfile_metadata_set_stringv (NemoFile *file,
                                            const char *keyfile_filename,
                                            const char *name,
                                            const char *key,
                                            const char * const *stringv);

gboolean nemo_keyfile_metadata_update_from_keyfile (NemoFile *file,
                                                        const char *keyfile_filename,
                                                        const gchar *name);

#endif /* __NEMO_KEYFILE_METADATA_H__ */
