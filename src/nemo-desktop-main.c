/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>,
 *          John Sullivan <sullivan@eazel.com>
 *
 */

/* nemo-main.c: Implementation of the routines that drive program lifecycle and main window creation/destruction. */

#include <config.h>

#include "nemo-desktop-application.h"

#include <libnemo-private/nemo-debug.h>
#include <libnemo-private/nemo-malloc-utils.h>
#include <eel/eel-debug.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gdesktopappinfo.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_EXEMPI
#include <exempi/xmp.h>
#endif

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <wayland-client.h>

static gboolean layer_shell_available = FALSE;

static void
registry_handle_global (void *data, struct wl_registry *registry,
                        uint32_t name, const char *interface, uint32_t version)
{
    if (g_strcmp0 (interface, "zwlr_layer_shell_v1") == 0)
        layer_shell_available = TRUE;
}

static void
registry_handle_global_remove (void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove,
};

static gboolean
check_layer_shell_support (void)
{
    struct wl_display *display;
    struct wl_registry *registry;

    display = wl_display_connect (NULL);
    if (!display)
        return FALSE;

    registry = wl_display_get_registry (display);
    wl_registry_add_listener (registry, &registry_listener, NULL);
    wl_display_roundtrip (display);

    wl_registry_destroy (registry);
    wl_display_disconnect (display);

    return layer_shell_available;
}
#endif

int
main (int argc, char *argv[])
{
	gint retval;
	NemoApplication *application;

	nemo_malloc_setup ();

	/* This will be done by gtk+ later, but for now, force it to GNOME */
	g_desktop_app_info_set_desktop_env ("GNOME");

	if (g_getenv ("NEMO_DEBUG") != NULL) {
		eel_make_warnings_and_criticals_stop_in_debugger ();
	}
	
	/* Initialize gettext support */
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname ("nemo-desktop");

#ifdef HAVE_GTK_LAYER_SHELL
	if (check_layer_shell_support ())
	{
		g_message ("nemo-desktop: using Wayland backend, gtk-layer-shell supported");
	}
	else
	{
		g_message ("nemo-desktop: Not a Wayland session, or wlr-layer-shell protocol not supported, using X11 backend");
		gdk_set_allowed_backends ("x11");
	}
#else
	g_message ("nemo-desktop: using X11");
	gdk_set_allowed_backends ("x11");
#endif

#ifdef HAVE_EXEMPI
	xmp_init();
#endif

	/* Run the nemo-desktop application. */
	application = nemo_desktop_application_get_singleton ();

    retval = g_application_run (G_APPLICATION (application),
                                argc, argv);

    g_object_unref (application);

    eel_debug_shut_down ();

    return retval;
}
