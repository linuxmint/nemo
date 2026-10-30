/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

/*
 * nemo-preview-image-info.c - Background probe of image files for the preview pane
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
#include "nemo-preview-image-info.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#include <libexif/exif-utils.h>
#include "nemo-exif-utils.h"
#endif

typedef struct {
    gchar *path;
    GDateTime *date_taken;  /* formatted on the main thread, where the date preferences live */
} FetchData;

static void
fetch_data_free (FetchData *data)
{
    g_free (data->path);
    g_clear_pointer (&data->date_taken, g_date_time_unref);
    g_free (data);
}

gboolean
nemo_preview_image_info_supported (NemoFile *file)
{
    gchar *mime_type;
    gchar *path;
    gboolean supported;

    mime_type = nemo_file_get_mime_type (file);
    supported = mime_type != NULL && g_str_has_prefix (mime_type, "image/");
    g_free (mime_type);

    if (supported) {
        path = nemo_file_get_activation_path (file);
        supported = path != NULL;
        g_free (path);
    }

    return supported;
}

#ifdef HAVE_EXIF
static gboolean
get_rational (ExifData *data,
              ExifTag   tag,
              gdouble  *value)
{
    ExifEntry *entry = exif_data_get_entry (data, tag);
    ExifRational rational;

    if (entry == NULL || entry->format != EXIF_FORMAT_RATIONAL || entry->components < 1) {
        return FALSE;
    }

    rational = exif_get_rational (entry->data, exif_data_get_byte_order (data));

    if (rational.numerator == 0 || rational.denominator == 0) {
        return FALSE;
    }

    *value = (gdouble) rational.numerator / rational.denominator;

    return TRUE;
}

static gchar *
get_stripped_string (ExifData *data,
                     ExifTag   tag)
{
    return nemo_preview_take_stripped (nemo_exif_get_tag_value_utf8 (data, tag));
}

static GDateTime *
get_date_taken (ExifData *data)
{
    GDateTime *date_time = NULL;
    gchar *value;
    gint year, month, day, hour, minute, second;

    value = get_stripped_string (data, EXIF_TAG_DATE_TIME_ORIGINAL);

    if (value == NULL) {
        value = get_stripped_string (data, EXIF_TAG_DATE_TIME_DIGITIZED);
    }

    /* EXIF dates carry no zone; treat them as local time, as cameras record them */
    if (value != NULL &&
        sscanf (value, "%d:%d:%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        date_time = g_date_time_new_local (year, month, day, hour, minute, second);
    }

    g_free (value);

    return date_time;
}

/* Many cameras repeat the make in the model ("Canon" + "Canon EOS 80D") */
static gchar *
get_camera (ExifData *data)
{
    gchar *make, *model, *camera;
    gsize make_word_len;

    make = get_stripped_string (data, EXIF_TAG_MAKE);
    model = get_stripped_string (data, EXIF_TAG_MODEL);

    if (make == NULL) {
        return model;
    }

    if (model == NULL) {
        return make;
    }

    make_word_len = strcspn (make, " ");

    if (g_ascii_strncasecmp (model, make, make_word_len) == 0) {
        camera = g_steal_pointer (&model);
    } else {
        camera = g_strdup_printf ("%s %s", make, model);
    }

    g_free (make);
    g_free (model);

    return camera;
}

static gchar *
get_exposure (ExifData *data)
{
    GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
    ExifEntry *entry;
    gdouble value;

    if (get_rational (data, EXIF_TAG_EXPOSURE_TIME, &value)) {
        gdouble reciprocal = 1.0 / value;

        /* As cameras show them: fractions for fast speeds, and slower ones that
         * aren't a whole fraction as decimals (0.4s, not 1/2.5s) */
        if (value <= 0.25 || (value < 1.0 && fabs (reciprocal - round (reciprocal)) < 0.01)) {
            /* Translators: an exposure time shorter than a second, e.g. "1/250 s" */
            g_ptr_array_add (parts, g_strdup_printf (_("1/%.0f s"), round (reciprocal)));
        } else {
            /* Translators: an exposure time in seconds, e.g. "2.5 s" */
            g_ptr_array_add (parts, g_strdup_printf (_("%g s"), round (value * 10) / 10));
        }
    }

    /* Two significant digits (f/2.8, f/11) without switching to exponent form at f/100+ */
    if (get_rational (data, EXIF_TAG_FNUMBER, &value)) {
        g_ptr_array_add (parts, g_strdup_printf ("f/%.*g", value < 100 ? 2 : 3, value));
    }

    entry = exif_data_get_entry (data, EXIF_TAG_ISO_SPEED_RATINGS);

    if (entry != NULL && entry->format == EXIF_FORMAT_SHORT && entry->components >= 1) {
        /* Translators: the ISO sensitivity of a photo, e.g. "ISO 200" */
        g_ptr_array_add (parts, g_strdup_printf (_("ISO %d"),
                                                 (gint) exif_get_short (entry->data, exif_data_get_byte_order (data))));
    }

    if (get_rational (data, EXIF_TAG_FOCAL_LENGTH, &value)) {
        /* Translators: a lens focal length, e.g. "35 mm" */
        g_ptr_array_add (parts, g_strdup_printf (_("%g mm"), round (value * 10) / 10));
    }

    return nemo_preview_join_parts (parts);
}

static void
read_exif (FetchData       *data,
           NemoPreviewInfo *info)
{
    ExifData *exif_data;
    gchar **rows = info->rows;

    exif_data = exif_data_new_from_file (data->path);

    if (exif_data == NULL) {
        return;
    }

    data->date_taken = get_date_taken (exif_data);
    rows[NEMO_PREVIEW_ROW_CAMERA] = get_camera (exif_data);
    rows[NEMO_PREVIEW_ROW_EXPOSURE] = get_exposure (exif_data);

    exif_data_unref (exif_data);
}
#endif

static void
fetch_thread (GTask        *task,
              gpointer      source_object,
              gpointer      task_data,
              GCancellable *cancellable)
{
    FetchData *data = task_data;
    NemoPreviewInfo *info;

    /* Skip fetches cancelled while still queued (e.g. arrowing through a folder) */
    if (g_task_return_error_if_cancelled (task)) {
        return;
    }

    info = g_new0 (NemoPreviewInfo, 1);

#ifdef HAVE_EXIF
    read_exif (data, info);
#endif

    g_task_return_pointer (task, info, (GDestroyNotify) nemo_preview_info_free);
}

void
nemo_preview_image_info_fetch (NemoFile            *file,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    GTask *task;
    FetchData *data;

    g_return_if_fail (NEMO_IS_FILE (file));

    data = g_new0 (FetchData, 1);
    data->path = nemo_file_get_activation_path (file);

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nemo_preview_image_info_fetch);
    g_task_set_task_data (task, data, (GDestroyNotify) fetch_data_free);
    g_task_run_in_thread (task, fetch_thread);
    g_object_unref (task);
}

NemoPreviewInfo *
nemo_preview_image_info_fetch_finish (GAsyncResult  *result,
                                      GError       **error)
{
    FetchData *data;
    NemoPreviewInfo *info;

    g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);
    g_return_val_if_fail (g_async_result_is_tagged (result, nemo_preview_image_info_fetch), NULL);

    info = g_task_propagate_pointer (G_TASK (result), error);

    if (info == NULL) {
        return NULL;
    }

    data = g_task_get_task_data (G_TASK (result));

    if (data->date_taken != NULL) {
        info->rows[NEMO_PREVIEW_ROW_DATE_TAKEN] = nemo_date_time_to_display_string (data->date_taken);
    }

    return info;
}
