/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-exif-utils.h - EXIF string helpers shared by the properties page and preview pane
 *
 * Copyright (C) 2026 Linux Mint
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
 */

#ifndef NEMO_EXIF_UTILS_H
#define NEMO_EXIF_UTILS_H

#include <glib.h>
#include <libexif/exif-data.h>

gchar *nemo_exif_string_to_utf8        (const gchar *exif_str);
gchar *nemo_exif_get_tag_value_utf8    (ExifData    *data,
                                        ExifTag      tag);

#endif /* NEMO_EXIF_UTILS_H */
