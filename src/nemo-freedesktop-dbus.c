/*
 * nemo-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
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
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */

#include <config.h>

#include "nemo-application.h"
#include "nemo-freedesktop-dbus.h"
#include "nemo-freedesktop-generated.h"

/* We share the same debug domain as nemo-dbus-manager */
#define DEBUG_FLAG NEMO_DEBUG_DBUS
#include <libnemo-private/nemo-debug.h>

#include "nemo-properties-window.h"

#include <gio/gio.h>

struct _NemoFreedesktopDBus {
	GObject parent;

	/* Id from g_dbus_own_name() */
	guint owner_id;

	/* DBus paraphernalia */
	GDBusObjectManagerServer *object_manager;

	/* Our DBus implementation skeleton */
	NemoFreedesktopFileManager1 *skeleton;
};

struct _NemoFreedesktopDBusClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (NemoFreedesktopDBus, nemo_freedesktop_dbus, G_TYPE_OBJECT);

static gboolean
skeleton_handle_show_items_cb (NemoFreedesktopFileManager1 *object,
			       GDBusMethodInvocation *invocation,
			       const gchar *const *uris,
			       const gchar *startup_id,
			       gpointer data)
{
        NemoApplication *application;
	int i;

        application = NEMO_APPLICATION (g_application_get_default ());

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;
		GFile *parent;

		file = g_file_new_for_uri (uris[i]);
		parent = g_file_get_parent (file);

		if (parent != NULL) {
			nemo_application_open_location (application, parent, file, startup_id, FALSE);
			g_object_unref (parent);
		} else {
			nemo_application_open_location (application, file, NULL, startup_id, FALSE);
		}

		g_object_unref (file);
	}

	nemo_freedesktop_file_manager1_complete_show_items (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (NemoFreedesktopFileManager1 *object,
				 GDBusMethodInvocation *invocation,
				 const gchar *const *uris,
				 const gchar *startup_id,
				 gpointer data)
{
        NemoApplication *application;
	int i;

        application = NEMO_APPLICATION (g_application_get_default ());

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;

		file = g_file_new_for_uri (uris[i]);

		nemo_application_open_location (application, file, NULL, startup_id, FALSE);

		g_object_unref (file);
	}

	nemo_freedesktop_file_manager1_complete_show_folders (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_item_properties_cb (NemoFreedesktopFileManager1 *object,
					 GDBusMethodInvocation *invocation,
					 const gchar *const *uris,
					 const gchar *startup_id,
					 gpointer data)
{
	GList *files;
	int i;

	files = NULL;

	for (i = 0; uris[i] != NULL; i++) {
		files = g_list_prepend (files, nemo_file_get_by_uri (uris[i]));
        }

	files = g_list_reverse (files);

	nemo_properties_window_present (files, NULL, startup_id);

	nemo_file_list_free (files);

	nemo_freedesktop_file_manager1_complete_show_item_properties (object, invocation);
	return TRUE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
		 const gchar     *name,
		 gpointer         user_data)
{
	NemoFreedesktopDBus *fdb = user_data;

	DEBUG ("Bus acquired at %s", name);

	fdb->object_manager = g_dbus_object_manager_server_new ("/org/freedesktop/FileManager1");

	fdb->skeleton = nemo_freedesktop_file_manager1_skeleton_new ();

	g_signal_connect (fdb->skeleton, "handle-show-items",
			  G_CALLBACK (skeleton_handle_show_items_cb), fdb);
	g_signal_connect (fdb->skeleton, "handle-show-folders",
			  G_CALLBACK (skeleton_handle_show_folders_cb), fdb);
	g_signal_connect (fdb->skeleton, "handle-show-item-properties",
			  G_CALLBACK (skeleton_handle_show_item_properties_cb), fdb);

	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (fdb->skeleton), conn, "/org/freedesktop/FileManager1", NULL);

	g_dbus_object_manager_server_set_connection (fdb->object_manager, conn);
}

static void
name_acquired_cb (GDBusConnection *connection,
		  const gchar     *name,
		  gpointer         user_data)
{
	DEBUG ("Acquired the name %s on the session message bus\n", name);
}

static void
name_lost_cb (GDBusConnection *connection,
	      const gchar     *name,
	      gpointer         user_data)
{
	DEBUG ("Lost (or failed to acquire) the name %s on the session message bus\n", name);
}

static void
nemo_freedesktop_dbus_dispose (GObject *object)
{
	NemoFreedesktopDBus *fdb = (NemoFreedesktopDBus *) object;

	if (fdb->owner_id != 0) {
		g_bus_unown_name (fdb->owner_id);
		fdb->owner_id = 0;
	}

	if (fdb->skeleton != NULL) {
		g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (fdb->skeleton));
		g_object_unref (fdb->skeleton);
		fdb->skeleton = NULL;
	}

	g_clear_object (&fdb->object_manager);

	G_OBJECT_CLASS (nemo_freedesktop_dbus_parent_class)->dispose (object);
}

static void
nemo_freedesktop_dbus_class_init (NemoFreedesktopDBusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = nemo_freedesktop_dbus_dispose;
}

static void
nemo_freedesktop_dbus_init (NemoFreedesktopDBus *fdb)
{
	fdb->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					"org.freedesktop.FileManager1",
					G_BUS_NAME_OWNER_FLAGS_NONE,
					bus_acquired_cb,
					name_acquired_cb,
					name_lost_cb,
					fdb,
					NULL);
}

void
nemo_freedesktop_dbus_set_open_locations (NemoFreedesktopDBus *fdb,
					      const gchar **locations)
{
	g_return_if_fail (NEMO_IS_FREEDESKTOP_DBUS (fdb));

	nemo_freedesktop_file_manager1_set_open_locations (fdb->skeleton, locations);
}

/* Tries to own the org.freedesktop.FileManager1 service name */
NemoFreedesktopDBus *
nemo_freedesktop_dbus_new (void)
{	
	return g_object_new (nemo_freedesktop_dbus_get_type (),
                             NULL);
}
