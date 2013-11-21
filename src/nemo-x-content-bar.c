/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Red Hat, Inc.
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 *          Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>

#include "nemo-x-content-bar.h"
#include <libnemo-private/nemo-icon-info.h>
#include <libnemo-private/nemo-program-choosing.h>

#define NEMO_X_CONTENT_BAR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NEMO_TYPE_X_CONTENT_BAR, NemoXContentBarPrivate))

struct NemoXContentBarPrivate
{
	GtkWidget *label;

	char **x_content_types;
	GMount *mount;
};

enum {
	PROP_0,
	PROP_MOUNT,
	PROP_X_CONTENT_TYPES,
};

enum {
	CONTENT_BAR_RESPONSE_APP = 1
};

G_DEFINE_TYPE (NemoXContentBar, nemo_x_content_bar, GTK_TYPE_INFO_BAR)

static void
content_bar_response_cb (GtkInfoBar *infobar,
			 gint response_id,
			 gpointer user_data)
{
	GAppInfo *default_app;
	NemoXContentBar *bar = user_data;

	if (response_id < 0) {
		return;
	}

	if (bar->priv->x_content_types == NULL ||
	    bar->priv->mount == NULL)
		return;

	/* FIXME */
 	default_app = g_app_info_get_default_for_type (bar->priv->x_content_types[response_id], FALSE);
	if (default_app != NULL) {
		nemo_launch_application_for_mount (default_app, bar->priv->mount,
						       GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (bar))));
		g_object_unref (default_app);
	}
}

static char *
get_message_for_x_content_type (const char *x_content_type)
{
	char *message;
	char *description;

	description = g_content_type_get_description (x_content_type);

	/* Customize greeting for well-known x-content types */
	/* translators: these describe the contents of removable media */
	if (strcmp (x_content_type, "x-content/audio-cdda") == 0) {
		message = g_strdup (_("Audio CD"));
	} else if (strcmp (x_content_type, "x-content/audio-dvd") == 0) {
		message = g_strdup (_("Audio DVD"));
	} else if (strcmp (x_content_type, "x-content/video-dvd") == 0) {
		message = g_strdup (_("Video DVD"));
	} else if (strcmp (x_content_type, "x-content/video-vcd") == 0) {
		message = g_strdup (_("Video CD"));
	} else if (strcmp (x_content_type, "x-content/video-svcd") == 0) {
		message = g_strdup (_("Super Video CD"));
	} else if (strcmp (x_content_type, "x-content/image-photocd") == 0) {
		message = g_strdup (_("Photo CD"));
	} else if (strcmp (x_content_type, "x-content/image-picturecd") == 0) {
		message = g_strdup (_("Picture CD"));
	} else if (strcmp (x_content_type, "x-content/image-dcf") == 0) {
		message = g_strdup (_("Contains digital photos"));
	} else if (strcmp (x_content_type, "x-content/audio-player") == 0) {
		message = g_strdup (_("Contains music"));
	} else if (strcmp (x_content_type, "x-content/software") == 0) {
		message = g_strdup (_("Contains software"));
	} else {
		/* fallback to generic greeting */
		message = g_strdup_printf (_("Detected as \"%s\""), description);
	}

	g_free (description);

	return message;
}

static char *
get_message_for_two_x_content_types (char **x_content_types)
{
	char *message;

	g_assert (x_content_types[0] != NULL);
	g_assert (x_content_types[1] != NULL);

	/* few combinations make sense */
	if (strcmp (x_content_types[0], "x-content/image-dcf") == 0
	    || strcmp (x_content_types[1], "x-content/image-dcf") == 0) {

		/* translators: these describe the contents of removable media */
		if (strcmp (x_content_types[0], "x-content/audio-player") == 0) {
			message = g_strdup (_("Contains music and photos"));
		} else if (strcmp (x_content_types[1], "x-content/audio-player") == 0) {
			message = g_strdup (_("Contains photos and music"));
		} else {
			message = g_strdup (_("Contains digital photos"));
		}
	} else {
		message = get_message_for_x_content_type (x_content_types[0]);
	}

	return message;
}

static void
nemo_x_content_bar_set_x_content_types (NemoXContentBar *bar, const char **x_content_types)
{
	char *message = NULL;
	guint num_types;
	guint n;
	GPtrArray *types;
	GPtrArray *apps;
	GAppInfo *default_app;

	g_strfreev (bar->priv->x_content_types);

	types = g_ptr_array_new ();
	apps = g_ptr_array_new ();
	g_ptr_array_set_free_func (apps, g_object_unref);
	for (n = 0; x_content_types[n] != NULL; n++) {
		if (g_str_has_prefix (x_content_types[n], "x-content/blank-"))
			continue;

		if (g_content_type_is_a (x_content_types[n], "x-content/win32-software"))
			continue;

		default_app = g_app_info_get_default_for_type (x_content_types[n], FALSE);
		if (default_app == NULL)
			continue;

		g_ptr_array_add (types, g_strdup (x_content_types[n]));
		g_ptr_array_add (apps, default_app);
	}

	num_types = types->len;
	g_ptr_array_add (types, NULL);

	bar->priv->x_content_types = (char **) g_ptr_array_free (types, FALSE);

	switch (num_types) {
	case 0:
		message = NULL;
		break;
	case 1:
		message = get_message_for_x_content_type (bar->priv->x_content_types[0]);
		break;
	case 2:
		message = get_message_for_two_x_content_types (bar->priv->x_content_types);
		break;
	default:
		message = g_strdup (_("Open with:"));
		break;
	}

	if (message == NULL) {
		g_ptr_array_free (apps, TRUE);
		gtk_widget_destroy (GTK_WIDGET (bar));
		return;
	}

	gtk_label_set_text (GTK_LABEL (bar->priv->label), message);
	g_free (message);

	gtk_widget_show (bar->priv->label);

	for (n = 0; bar->priv->x_content_types[n] != NULL; n++) {
		const char *name;
		GIcon *icon;
		GtkWidget *image;
		GtkWidget *button;

		/* TODO: We really need a GtkBrowserBackButton-ish widget here.. until then, we only
		 *       show the default application. */

		default_app = g_ptr_array_index (apps, n);
		icon = g_app_info_get_icon (default_app);
		if (icon != NULL) {
			image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
		} else {
			image = NULL;
		}

		name = g_app_info_get_name (default_app);
		button = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
						  name,
						  n);

		gtk_button_set_image (GTK_BUTTON (button), image);
#if GTK_CHECK_VERSION(3,6,0)
		gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
#endif
		gtk_button_set_label (GTK_BUTTON (button), name);
		gtk_widget_show (button);
	}

	g_ptr_array_free (apps, TRUE);
}

static void
nemo_x_content_bar_set_mount (NemoXContentBar *bar, GMount *mount)
{
	if (bar->priv->mount != NULL) {
		g_object_unref (bar->priv->mount);
	}
	bar->priv->mount = mount != NULL ? g_object_ref (mount) : NULL;
}


static void
nemo_x_content_bar_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
	NemoXContentBar *bar;

	bar = NEMO_X_CONTENT_BAR (object);

	switch (prop_id) {
	case PROP_MOUNT:
		nemo_x_content_bar_set_mount (bar, G_MOUNT (g_value_get_object (value)));
		break;
	case PROP_X_CONTENT_TYPES:
		nemo_x_content_bar_set_x_content_types (bar, g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nemo_x_content_bar_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
	NemoXContentBar *bar;

	bar = NEMO_X_CONTENT_BAR (object);

	switch (prop_id) {
	case PROP_MOUNT:
                g_value_set_object (value, bar->priv->mount);
		break;
	case PROP_X_CONTENT_TYPES:
		g_value_set_boxed (value, &bar->priv->x_content_types);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nemo_x_content_bar_finalize (GObject *object)
{
	NemoXContentBar *bar = NEMO_X_CONTENT_BAR (object);

	g_strfreev (bar->priv->x_content_types);
	if (bar->priv->mount != NULL)
		g_object_unref (bar->priv->mount);

        G_OBJECT_CLASS (nemo_x_content_bar_parent_class)->finalize (object);
}

static void
nemo_x_content_bar_class_init (NemoXContentBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->get_property = nemo_x_content_bar_get_property;
	object_class->set_property = nemo_x_content_bar_set_property;
	object_class->finalize = nemo_x_content_bar_finalize;

	g_type_class_add_private (klass, sizeof (NemoXContentBarPrivate));

        g_object_class_install_property (object_class,
					 PROP_MOUNT,
					 g_param_spec_object (
						 "mount",
						 "The GMount to run programs for",
						 "The GMount to run programs for",
						 G_TYPE_MOUNT,
						 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

        g_object_class_install_property (object_class,
					 PROP_X_CONTENT_TYPES,
					 g_param_spec_boxed ("x-content-types",
							     "The x-content types for the cluebar",
							     "The x-content types for the cluebar",
							     G_TYPE_STRV,
							     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
nemo_x_content_bar_init (NemoXContentBar *bar)
{
	GtkWidget *content_area;
	GtkWidget *action_area;
	PangoAttrList *attrs;

	bar->priv = NEMO_X_CONTENT_BAR_GET_PRIVATE (bar);
	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));
	action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area), GTK_ORIENTATION_HORIZONTAL);

	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	bar->priv->label = gtk_label_new (NULL);
	gtk_label_set_attributes (GTK_LABEL (bar->priv->label), attrs);
	pango_attr_list_unref (attrs);

	gtk_label_set_ellipsize (GTK_LABEL (bar->priv->label), PANGO_ELLIPSIZE_END);
	gtk_container_add (GTK_CONTAINER (content_area), bar->priv->label);

	g_signal_connect (bar, "response",
			  G_CALLBACK (content_bar_response_cb),
			  bar);
}

GtkWidget *
nemo_x_content_bar_new (GMount *mount,
			    const char **x_content_types)
{
	return g_object_new (NEMO_TYPE_X_CONTENT_BAR,
			     "message-type", GTK_MESSAGE_QUESTION,
			     "mount", mount,
			     "x-content-types", x_content_types,
			     NULL);
}
