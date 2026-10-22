/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-preview-pane.c - Container widget for preview pane
 *
 * Copyright (C) 2025 Linux Mint
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
#include "nemo-preview-pane.h"
#include <libnemo-private/nemo-preview-image.h>
#include <libnemo-private/nemo-preview-details.h>
#ifdef HAVE_GSTREAMER
#include <libnemo-private/nemo-preview-video.h>
#include <libnemo-private/nemo-preview-media-info.h>
#endif
#include <libnemo-private/nemo-global-preferences.h>
#include <glib/gi18n.h>

#define PREVIEW_IMAGE_HEIGHT 200

struct _NemoPreviewPane {
	GtkBox parent;
};

typedef struct {
	GtkWidget *vpaned;
	GtkWidget *top_stack;
	GtkWidget *image_widget;
	GtkWidget *video_widget;
	GtkWidget *details_widget;
	GtkWidget *empty_label;

	NemoFile *current_file;
	gulong file_changed_id;
	GCancellable *media_cancellable;

	gboolean initial_position_set;
} NemoPreviewPanePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NemoPreviewPane, nemo_preview_pane, GTK_TYPE_BOX)

static void
vpaned_size_allocate_callback (GtkWidget *widget, GtkAllocation *allocation, gpointer user_data)
{
	NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
	NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
	gint saved_height, position;

	/* Only set initial position once */
	if (priv->initial_position_set) {
		return;
	}

	priv->initial_position_set = TRUE;

	/* Set position based on saved details height */
	saved_height = g_settings_get_int (nemo_preview_pane_preferences, "details-height");
	if (saved_height > 50 && allocation->height > saved_height) {
		/* Position is from top, so subtract details height from total */
		position = allocation->height - saved_height;
		gtk_paned_set_position (GTK_PANED (widget), position);
	} else {
		/* Fallback: make image section PREVIEW_IMAGE_HEIGHT */
		gtk_paned_set_position (GTK_PANED (widget), PREVIEW_IMAGE_HEIGHT);
	}
}

static void
details_pane_position_changed_callback (GObject *paned, GParamSpec *pspec, gpointer user_data)
{
	NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
	NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
	gint position, total_height, details_height;

	/* Don't save position until initial position has been set */
	if (!priv->initial_position_set) {
		return;
	}

	position = gtk_paned_get_position (GTK_PANED (paned));
	total_height = gtk_widget_get_allocated_height (GTK_WIDGET (paned));

	/* Calculate height of details pane (bottom side) */
	details_height = total_height - position;

	/* Only save if details height is reasonable */
	if (details_height > 50 && total_height > 0) {
		g_settings_set_int (nemo_preview_pane_preferences, "details-height", details_height);
	}
}

static void
cancel_media_fetch (NemoPreviewPane *pane)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    if (priv->media_cancellable != NULL) {
        g_cancellable_cancel (priv->media_cancellable);
        g_clear_object (&priv->media_cancellable);
    }
}

#ifdef HAVE_GSTREAMER
static void
media_info_ready (GObject      *source_object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
    NemoPreviewPane *pane = NEMO_PREVIEW_PANE (user_data);
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);
    NemoPreviewMediaInfo *info;

    info = nemo_preview_media_info_fetch_finish (result, NULL);

    if (info != NULL) {
        nemo_preview_details_set_media_info (NEMO_PREVIEW_DETAILS (priv->details_widget),
                                             info->duration, info->dimensions);

        if (info->cover != NULL && priv->video_widget != NULL) {
            nemo_preview_video_set_cover (NEMO_PREVIEW_VIDEO (priv->video_widget), info->cover);
        }

        nemo_preview_media_info_free (info);
    }

    g_object_unref (pane);
}

static void
start_media_fetch (NemoPreviewPane *pane,
                   NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

    cancel_media_fetch (pane);

    if (!nemo_preview_media_info_supported (file)) {
        return;
    }

    priv->media_cancellable = g_cancellable_new ();
    nemo_preview_media_info_fetch (file, priv->media_cancellable, media_info_ready, g_object_ref (pane));
}
#endif

static void
update_top_widget (NemoPreviewPane *pane,
                   NemoFile        *file)
{
    NemoPreviewPanePrivate *priv = nemo_preview_pane_get_instance_private (pane);

#ifdef HAVE_GSTREAMER
    if (priv->video_widget != NULL) {
        if (nemo_preview_media_info_supported (file)) {
            nemo_preview_image_clear (NEMO_PREVIEW_IMAGE (priv->image_widget));
            nemo_preview_video_set_file (NEMO_PREVIEW_VIDEO (priv->video_widget), file);
            gtk_stack_set_visible_child (GTK_STACK (priv->top_stack), priv->video_widget);
            return;
        }

        nemo_preview_video_clear (NEMO_PREVIEW_VIDEO (priv->video_widget));
    }
#endif

    nemo_preview_image_set_file (NEMO_PREVIEW_IMAGE (priv->image_widget), file);
    gtk_stack_set_visible_child (GTK_STACK (priv->top_stack), priv->image_widget);
}

static void
file_changed_callback (NemoFile *file, gpointer user_data)
{
	NemoPreviewPane *pane;
	NemoPreviewPanePrivate *priv;

	pane = NEMO_PREVIEW_PANE (user_data);
	priv = nemo_preview_pane_get_instance_private (pane);

	/* Refresh the preview if the file has changed */
	if (file == priv->current_file) {
		nemo_preview_pane_set_file (pane, file);
	}
}

static void
nemo_preview_pane_finalize (GObject *object)
{
	NemoPreviewPane *pane;
	NemoPreviewPanePrivate *priv;

	pane = NEMO_PREVIEW_PANE (object);
	priv = nemo_preview_pane_get_instance_private (pane);

	cancel_media_fetch (pane);

	if (priv->current_file != NULL) {
		if (priv->file_changed_id != 0) {
			g_signal_handler_disconnect (priv->current_file,
			                              priv->file_changed_id);
			priv->file_changed_id = 0;
		}
		nemo_file_unref (priv->current_file);
		priv->current_file = NULL;
	}

	G_OBJECT_CLASS (nemo_preview_pane_parent_class)->finalize (object);
}

static void
nemo_preview_pane_init (NemoPreviewPane *pane)
{
	NemoPreviewPanePrivate *priv;
	GtkWidget *scrolled;

	priv = nemo_preview_pane_get_instance_private (pane);

	/* Create empty state label */
	priv->empty_label = gtk_label_new (_("No file selected"));
	gtk_widget_set_halign (priv->empty_label, GTK_ALIGN_CENTER);
	gtk_widget_set_valign (priv->empty_label, GTK_ALIGN_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (priv->empty_label),
	                              "dim-label");
	gtk_box_pack_start (GTK_BOX (pane), priv->empty_label, TRUE, TRUE, 0);
	gtk_widget_show (priv->empty_label);

	/* Create vertical paned widget */
	priv->vpaned = gtk_paned_new (GTK_ORIENTATION_VERTICAL);
	gtk_box_pack_start (GTK_BOX (pane), priv->vpaned, TRUE, TRUE, 0);

    priv->top_stack = gtk_stack_new ();
    gtk_paned_pack1 (GTK_PANED (priv->vpaned), priv->top_stack, FALSE, FALSE);

    priv->image_widget = nemo_preview_image_new ();
    gtk_stack_add_named (GTK_STACK (priv->top_stack), priv->image_widget, "image");

#ifdef HAVE_GSTREAMER
    if (nemo_preview_video_available ()) {
        priv->video_widget = nemo_preview_video_new ();
        gtk_stack_add_named (GTK_STACK (priv->top_stack), priv->video_widget, "video");
    }
#endif

    gtk_widget_show_all (priv->top_stack);

	/* Create details widget (bottom) in a scrolled window */
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
	                                 GTK_POLICY_NEVER,
	                                 GTK_POLICY_AUTOMATIC);

	priv->details_widget = nemo_preview_details_new ();
	gtk_container_add (GTK_CONTAINER (scrolled), priv->details_widget);
	gtk_widget_show (priv->details_widget);

	gtk_paned_pack2 (GTK_PANED (priv->vpaned),
	                 scrolled, TRUE, FALSE);
	gtk_widget_show (scrolled);

	/* Initialize flag */
	priv->initial_position_set = FALSE;

	/* Connect size-allocate to set initial position from saved settings */
	g_signal_connect (priv->vpaned, "size-allocate",
	                  G_CALLBACK (vpaned_size_allocate_callback),
	                  pane);

	/* Connect signal to save position on resize */
	g_signal_connect (priv->vpaned, "notify::position",
	                  G_CALLBACK (details_pane_position_changed_callback),
	                  pane);
}

static void
nemo_preview_pane_class_init (NemoPreviewPaneClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = nemo_preview_pane_finalize;
}

GtkWidget *
nemo_preview_pane_new (void)
{
	return g_object_new (NEMO_TYPE_PREVIEW_PANE,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     "spacing", 0,
			     NULL);
}

void
nemo_preview_pane_set_file (NemoPreviewPane *pane,
                             NemoFile        *file)
{
	NemoPreviewPanePrivate *priv;

	g_return_if_fail (NEMO_IS_PREVIEW_PANE (pane));

	priv = nemo_preview_pane_get_instance_private (pane);

	if (file != NULL && file == priv->current_file) {
		return;
	}

	/* Disconnect from previous file if necessary */
	if (priv->current_file != NULL) {
		if (priv->file_changed_id != 0) {
			g_signal_handler_disconnect (priv->current_file,
			                              priv->file_changed_id);
			priv->file_changed_id = 0;
		}
		nemo_file_unref (priv->current_file);
		priv->current_file = NULL;
	}

	priv->current_file = file;

	if (file != NULL) {
		nemo_file_ref (file);

		/* Monitor file for changes */
		priv->file_changed_id =
			g_signal_connect (file, "changed",
			                  G_CALLBACK (file_changed_callback),
			                  pane);

        update_top_widget (pane, file);
		nemo_preview_details_set_file (NEMO_PREVIEW_DETAILS (priv->details_widget), file);
#ifdef HAVE_GSTREAMER
        start_media_fetch (pane, file);
#endif

		/* Show preview, hide empty label */
		gtk_widget_hide (priv->empty_label);
		gtk_widget_show (priv->vpaned);
	} else {
		/* No file selected - show empty state */
		nemo_preview_pane_clear (pane);
	}
}

void
nemo_preview_pane_clear (NemoPreviewPane *pane)
{
	NemoPreviewPanePrivate *priv;

	g_return_if_fail (NEMO_IS_PREVIEW_PANE (pane));

	priv = nemo_preview_pane_get_instance_private (pane);

	cancel_media_fetch (pane);

	if (priv->current_file != NULL) {
		if (priv->file_changed_id != 0) {
			g_signal_handler_disconnect (priv->current_file,
			                              priv->file_changed_id);
			priv->file_changed_id = 0;
		}
		nemo_file_unref (priv->current_file);
		priv->current_file = NULL;
	}

	nemo_preview_image_clear (NEMO_PREVIEW_IMAGE (priv->image_widget));
#ifdef HAVE_GSTREAMER
    if (priv->video_widget != NULL) {
        nemo_preview_video_clear (NEMO_PREVIEW_VIDEO (priv->video_widget));
    }
#endif
	nemo_preview_details_clear (NEMO_PREVIEW_DETAILS (priv->details_widget));

	/* Hide preview, show empty label */
	gtk_widget_hide (priv->vpaned);
	gtk_widget_show (priv->empty_label);
}

gboolean
nemo_preview_pane_toggle_playback (NemoPreviewPane *pane)
{
    NemoPreviewPanePrivate *priv;

    g_return_val_if_fail (NEMO_IS_PREVIEW_PANE (pane), FALSE);

    priv = nemo_preview_pane_get_instance_private (pane);

#ifdef HAVE_GSTREAMER
    if (priv->video_widget != NULL &&
        gtk_stack_get_visible_child (GTK_STACK (priv->top_stack)) == priv->video_widget) {
        return nemo_preview_video_toggle_playback (NEMO_PREVIEW_VIDEO (priv->video_widget));
    }
#endif

    return FALSE;
}
