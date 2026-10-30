/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-exif-utils.c - EXIF string helpers shared by the properties page and preview pane
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

#include <config.h>
#include "nemo-exif-utils.h"
#include <eel/eel-vfs-extensions.h>

gchar *
nemo_exif_string_to_utf8 (const gchar *exif_str)
{
    gchar *utf8_str;

    if (g_utf8_validate (exif_str, -1, NULL)) {
        return g_strdup (exif_str);
    }

    utf8_str = g_locale_to_utf8 (exif_str, -1, NULL, NULL, NULL);

    if (utf8_str != NULL) {
        return utf8_str;
    }

    return eel_make_valid_utf8 (exif_str);
}

gchar *
nemo_exif_get_tag_value_utf8 (ExifData *data,
                              ExifTag   tag)
{
    ExifEntry *entry;
    gchar buffer[1024];

    entry = exif_data_get_entry (data, tag);

    if (entry == NULL || exif_entry_get_value (entry, buffer, sizeof (buffer)) == NULL) {
        return NULL;
    }

    return nemo_exif_string_to_utf8 (buffer);
}
