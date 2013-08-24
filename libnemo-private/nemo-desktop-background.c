/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-desktop-background.c: Helper object to handle desktop background
 *                                changes.
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>

#include "nemo-desktop-background.h"

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-extensions.h>

#include "nemo-global-preferences.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libcinnamon-desktop/gnome-bg.h>
#include <gdesktop-enums.h>

#include <gtk/gtk.h>
#include <string.h>

static void init_fade (NemoDesktopBackground *self);
static void free_fade (NemoDesktopBackground *self);
static void queue_background_change (NemoDesktopBackground *self);

static NemoDesktopBackground *singleton = NULL;

G_DEFINE_TYPE (NemoDesktopBackground, nemo_desktop_background, G_TYPE_OBJECT);

enum {
        PROP_WIDGET = 1,
        NUM_PROPERTIES,
};

struct NemoDesktopBackgroundDetails {

	GtkWidget *widget;
        GnomeBG *bg;

	/* Realized data: */
	cairo_surface_t *background_surface;
	GnomeBGCrossfade *fade;
	int background_entire_width;
	int background_entire_height;
	GdkColor default_color;

	/* Desktop screen size watcher */
	gulong screen_size_handler;
	/* Desktop monitors configuration watcher */
	gulong screen_monitors_handler;
	guint change_idle_id;
};


static gboolean
background_settings_change_event_cb (GSettings *settings,
                                     gpointer   keys,
                                     gint       n_keys,
                                     gpointer   user_data);


static void
free_fade (NemoDesktopBackground *self)
{
	if (self->details->fade != NULL) {
		g_object_unref (self->details->fade);
		self->details->fade = NULL;
	}
}

static void
free_background_surface (NemoDesktopBackground *self)
{
	cairo_surface_t *surface;

	surface = self->details->background_surface;
	if (surface != NULL) {
		cairo_surface_destroy (surface);
		self->details->background_surface = NULL;
	}
}

static void
nemo_desktop_background_finalize (GObject *object)
{
	NemoDesktopBackground *self;

	self = NEMO_DESKTOP_BACKGROUND (object);

	g_signal_handlers_disconnect_by_func (gnome_background_preferences,
					      background_settings_change_event_cb,
					      self);

	free_background_surface (self);
	free_fade (self);

	g_clear_object (&self->details->bg);

	G_OBJECT_CLASS (nemo_desktop_background_parent_class)->finalize (object);
}

static void
nemo_desktop_background_unrealize (NemoDesktopBackground *self)
{
	free_background_surface (self);

	self->details->background_entire_width = 0;
	self->details->background_entire_height = 0;
	self->details->default_color.red = 0xffff;
	self->details->default_color.green = 0xffff;
	self->details->default_color.blue = 0xffff;
}

static void
nemo_desktop_background_set_image_uri (NemoDesktopBackground *self,
                                           const char *image_uri)
{
	char *filename;

	if (image_uri != NULL) {
		filename = g_filename_from_uri (image_uri, NULL, NULL);
	}
	else {
		filename = NULL;
	}

	gnome_bg_set_filename (self->details->bg, filename);

	g_free (filename);
}

static void
init_fade (NemoDesktopBackground *self)
{
	GtkWidget *widget;
	gboolean do_fade;

	widget = self->details->widget;

	if (widget == NULL || !gtk_widget_get_realized (widget))
		return;

	do_fade = g_settings_get_boolean (nemo_desktop_preferences,
					  NEMO_PREFERENCES_DESKTOP_BACKGROUND_FADE);

	if (!do_fade) {
		return;
	}

	if (self->details->fade == NULL) {
		GdkWindow *window;
		GdkScreen *screen;
		int old_width, old_height, width, height;

		/* If this was the result of a screen size change,
		 * we don't want to crossfade
		 */
		window = gtk_widget_get_window (widget);
		old_width = gdk_window_get_width (window);
		old_height = gdk_window_get_height (window);

		screen = gtk_widget_get_screen (widget);
		width = gdk_screen_get_width (screen);
		height = gdk_screen_get_height (screen);

		if (old_width == width && old_height == height) {
			self->details->fade = gnome_bg_crossfade_new (width, height);
			g_signal_connect_swapped (self->details->fade,
                                                  "finished",
                                                  G_CALLBACK (free_fade),
                                                  self);
		}
	}

	if (self->details->fade != NULL && !gnome_bg_crossfade_is_started (self->details->fade)) {
		cairo_surface_t *start_surface;

		if (self->details->background_surface == NULL) {
			start_surface = gnome_bg_get_surface_from_root (gtk_widget_get_screen (widget));
		} else {
			start_surface = cairo_surface_reference (self->details->background_surface);
		}
		gnome_bg_crossfade_set_start_surface (self->details->fade,
						      start_surface);
                cairo_surface_destroy (start_surface);
	}
}

static void
screen_size_changed (GdkScreen *screen,
                     NemoDesktopBackground *self)
{
	queue_background_change (self);
}

static gboolean
nemo_desktop_background_ensure_realized (NemoDesktopBackground *self)
{
	int entire_width;
	int entire_height;
	GdkScreen *screen;
	GdkWindow *window;

	screen = gtk_widget_get_screen (self->details->widget);
	entire_height = gdk_screen_get_height (screen);
	entire_width = gdk_screen_get_width (screen);

	/* If the window size is the same as last time, don't update */
	if (entire_width == self->details->background_entire_width &&
	    entire_height == self->details->background_entire_height) {
		return FALSE;
	}

	free_background_surface (self);

	window = gtk_widget_get_window (self->details->widget);
	self->details->background_surface = gnome_bg_create_surface (self->details->bg,
                                                                     window,
                                                                     entire_width, entire_height,
                                                                     TRUE);

	/* We got the surface and everything, so we don't care about a change
	   that is pending (unless things actually change after this time) */
	g_object_set_data (G_OBJECT (self),
			   "ignore-pending-change", GINT_TO_POINTER (TRUE));

	self->details->background_entire_width = entire_width;
	self->details->background_entire_height = entire_height;

	return TRUE;
}

static void
on_fade_finished (GnomeBGCrossfade *fade,
		  GdkWindow *window,
		  gpointer user_data)
{
        NemoDesktopBackground *self = user_data;

	nemo_desktop_background_ensure_realized (self);
	gnome_bg_set_surface_as_root (gdk_window_get_screen (window),
                                      self->details->background_surface);
}

static gboolean
fade_to_surface (NemoDesktopBackground *self,
		 GdkWindow     *window,
		 cairo_surface_t *surface)
{
	if (self->details->fade == NULL) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_set_end_surface (self->details->fade,
				                 surface)) {
		return FALSE;
	}

	if (!gnome_bg_crossfade_is_started (self->details->fade)) {
		gnome_bg_crossfade_start (self->details->fade, window);
		g_signal_connect (self->details->fade,
				  "finished",
				  G_CALLBACK (on_fade_finished), self);
	}

	return gnome_bg_crossfade_is_started (self->details->fade);
}

static void
nemo_desktop_background_set_up_widget (NemoDesktopBackground *self)
{
	GdkWindow *window;
	gboolean in_fade = FALSE;
        GtkWidget *widget;

        widget = self->details->widget;

	if (!gtk_widget_get_realized (widget)) {
		return;
	}

	nemo_desktop_background_ensure_realized (self);
        window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));

	in_fade = fade_to_surface (self, window,
				   self->details->background_surface);

	if (!in_fade) {
		cairo_pattern_t *pattern;

		pattern = cairo_pattern_create_for_surface (self->details->background_surface);
		gdk_window_set_background_pattern (window, pattern);
		cairo_pattern_destroy (pattern);

                gnome_bg_set_surface_as_root (gtk_widget_get_screen (widget),
                                              self->details->background_surface);
	}
}

static gboolean
background_changed_cb (NemoDesktopBackground *self)
{
	self->details->change_idle_id = 0;

	nemo_desktop_background_unrealize (self);
	nemo_desktop_background_set_up_widget (self);

	gtk_widget_queue_draw (self->details->widget);

	return FALSE;
}

static void
queue_background_change (NemoDesktopBackground *self)
{
	if (self->details->change_idle_id != 0) {
                g_source_remove (self->details->change_idle_id);
	}

	self->details->change_idle_id =
                g_idle_add ((GSourceFunc) background_changed_cb, self);
}

static void
nemo_desktop_background_changed (GnomeBG *bg,
                                     gpointer user_data)
{
        NemoDesktopBackground *self;

        self = user_data;
	init_fade (self);
	queue_background_change (self);
}

static void
nemo_desktop_background_transitioned (GnomeBG *bg,
                                          gpointer user_data)
{
        NemoDesktopBackground *self;

        self = user_data;
	free_fade (self);
	queue_background_change (self);
}

static void
widget_realize_cb (GtkWidget *widget,
                   gpointer user_data)
{
	GdkScreen *screen;
        NemoDesktopBackground *self = user_data;

	screen = gtk_widget_get_screen (widget);

	if (self->details->screen_size_handler > 0) {
		g_signal_handler_disconnect (screen,
					     self->details->screen_size_handler);
	}
	self->details->screen_size_handler = 
		g_signal_connect (screen, "size_changed",
				  G_CALLBACK (screen_size_changed), self);

	if (self->details->screen_monitors_handler > 0) {
		g_signal_handler_disconnect (screen,
					     self->details->screen_monitors_handler);
	}
	self->details->screen_monitors_handler =
		g_signal_connect (screen, "monitors-changed",
				  G_CALLBACK (screen_size_changed), self);
	nemo_desktop_background_set_up_widget (self);
}

static void
widget_unrealize_cb (GtkWidget *widget,
                     gpointer user_data)
{
        NemoDesktopBackground *self = user_data;

	if (self->details->screen_size_handler > 0) {
		        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
				                     self->details->screen_size_handler);
			self->details->screen_size_handler = 0;
	}
	if (self->details->screen_monitors_handler > 0) {
		        g_signal_handler_disconnect (gtk_widget_get_screen (GTK_WIDGET (widget)),
				                     self->details->screen_monitors_handler);
			self->details->screen_monitors_handler = 0;
	}
}

static void
on_widget_destroyed (GtkWidget *widget,
                     gpointer user_data)
{
        NemoDesktopBackground *self = user_data;

	if (self->details->change_idle_id != 0) {
		g_source_remove (self->details->change_idle_id);
		self->details->change_idle_id = 0;
	}

	free_fade (self);
	self->details->widget = NULL;
}

static gboolean
background_change_event_idle_cb (NemoDesktopBackground *self)
{
	gnome_bg_load_from_preferences (self->details->bg,
					gnome_background_preferences);

	g_object_unref (self);

	return FALSE;
}

static gboolean
background_settings_change_event_cb (GSettings *settings,
                                     gpointer   keys,
                                     gint       n_keys,
                                     gpointer   user_data)
{
	NemoDesktopBackground *self = user_data;

	/* Need to defer signal processing otherwise
	 * we would make the dconf backend deadlock.
	 */
	g_idle_add ((GSourceFunc) background_change_event_idle_cb,
		    g_object_ref (self));

	return FALSE;
}

static void
nemo_desktop_background_constructed (GObject *obj)
{
        NemoDesktopBackground *self;
        GtkWidget *widget;

        self = NEMO_DESKTOP_BACKGROUND (obj);

        if (G_OBJECT_CLASS (nemo_desktop_background_parent_class)->constructed != NULL) {
                G_OBJECT_CLASS (nemo_desktop_background_parent_class)->constructed (obj);
        }

        widget = self->details->widget;

        g_assert (widget != NULL);

 	g_signal_connect_object (widget, "destroy",
                                 G_CALLBACK (on_widget_destroyed), self, 0);
	g_signal_connect_object (widget, "realize",
				 G_CALLBACK (widget_realize_cb), self, 0);
	g_signal_connect_object (widget, "unrealize",
				 G_CALLBACK (widget_unrealize_cb), self, 0);

        gnome_bg_load_from_preferences (self->details->bg,
                                        gnome_background_preferences);

        /* Let's receive batch change events instead of every single one */
        g_signal_connect (gnome_background_preferences,
                          "change-event",
                          G_CALLBACK (background_settings_change_event_cb),
                          self);

	queue_background_change (self);
}

static void
nemo_desktop_background_set_property (GObject *object,
                                          guint property_id,
                                          const GValue *value,
                                          GParamSpec *pspec)
{
        NemoDesktopBackground *self;

        self = NEMO_DESKTOP_BACKGROUND (object);

        switch (property_id) {
        case PROP_WIDGET:
                self->details->widget = g_value_get_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
                break;
        }
}

static GObject *
nemo_desktop_background_constructor (GType type,
                                         guint n_construct_params,
                                         GObjectConstructParam *construct_params)
{
        GObject *retval;

        if (singleton != NULL) {
                return g_object_ref (singleton);
        }

        retval = G_OBJECT_CLASS (nemo_desktop_background_parent_class)->constructor
                (type, n_construct_params, construct_params);

        singleton = NEMO_DESKTOP_BACKGROUND (retval);
        g_object_add_weak_pointer (retval, (gpointer) &singleton);

        return retval;
}

static void
nemo_desktop_background_class_init (NemoDesktopBackgroundClass *klass)
{
	GObjectClass *object_class;
        GParamSpec *pspec;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = nemo_desktop_background_finalize;
        object_class->set_property = nemo_desktop_background_set_property;
        object_class->constructor = nemo_desktop_background_constructor;
        object_class->constructed = nemo_desktop_background_constructed;

        pspec = g_param_spec_object ("widget", "The widget for this background",
                                     "The widget that gets its background set",
                                     NEMO_TYPE_ICON_CONTAINER,
                                     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
        g_object_class_install_property (object_class, PROP_WIDGET, pspec);

	g_type_class_add_private (klass, sizeof (NemoDesktopBackgroundDetails));
}

static void
nemo_desktop_background_init (NemoDesktopBackground *self)
{
	self->details =
		G_TYPE_INSTANCE_GET_PRIVATE (self,
					     NEMO_TYPE_DESKTOP_BACKGROUND,
					     NemoDesktopBackgroundDetails);

        self->details->bg = gnome_bg_new ();
	self->details->default_color.red = 0xffff;
	self->details->default_color.green = 0xffff;
	self->details->default_color.blue = 0xffff;

	g_signal_connect (self->details->bg, "changed",
			  G_CALLBACK (nemo_desktop_background_changed), self);
	g_signal_connect (self->details->bg, "transitioned",
			  G_CALLBACK (nemo_desktop_background_transitioned), self);
}

NemoDesktopBackground *
nemo_desktop_background_new (NemoIconContainer *container)
{
        return g_object_new (NEMO_TYPE_DESKTOP_BACKGROUND,
                             "widget", container,
                             NULL);
}
