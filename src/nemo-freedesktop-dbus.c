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

#include "nemo-freedesktop-dbus.h"
#include "nemo-freedesktop-generated.h"

/* We share the same debug domain as nemo-dbus-manager */
#define DEBUG_FLAG NEMO_DEBUG_DBUS
#include <libnemo-private/nemo-debug.h>

#include "nemo-properties-window.h"

#include <gio/gio.h>


typedef struct _NemoFreedesktopDBus NemoFreedesktopDBus;
typedef struct _NemoFreedesktopDBusClass NemoFreedesktopDBusClass;

struct _NemoFreedesktopDBus {
	GObject parent;

	/* Parent application */
	NemoApplication *application;

	/* Id from g_dbus_own_name() */
	guint owner_id;

	/* DBus paraphernalia */
	GDBusConnection *connection;
	GDBusObjectManagerServer *object_manager;

	/* Our DBus implementation skeleton */
	NemoFreedesktopNemoFileManager1 *skeleton;
};

struct _NemoFreedesktopDBusClass {
	GObjectClass parent_class;
};

enum {
	PROP_APPLICATION = 1
};

#define SERVICE_TIMEOUT 5

static GType nemo_freedesktop_dbus_get_type (void) G_GNUC_CONST;
G_DEFINE_TYPE (NemoFreedesktopDBus, nemo_freedesktop_dbus, G_TYPE_OBJECT);

static NemoFreedesktopDBus *singleton = NULL;

static gboolean
skeleton_handle_show_items_cb (NemoFreedesktopNemoFileManager1 *object,
			       GDBusMethodInvocation *invocation,
			       const gchar *const *uris,
			       const gchar *startup_id,
			       gpointer data)
{
	NemoFreedesktopDBus *fdb = data;
	int i;

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;
		GFile *parent;

		file = g_file_new_for_uri (uris[i]);
		parent = g_file_get_parent (file);

		if (parent != NULL) {
			nemo_application_open_location (fdb->application, parent, file, startup_id);
			g_object_unref (parent);
		} else {
			nemo_application_open_location (fdb->application, file, NULL, startup_id);
		}

		g_object_unref (file);
	}

	nemo_freedesktop_nemo_file_manager1_complete_show_items (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_folders_cb (NemoFreedesktopNemoFileManager1 *object,
				 GDBusMethodInvocation *invocation,
				 const gchar *const *uris,
				 const gchar *startup_id,
				 gpointer data)
{
	NemoFreedesktopDBus *fdb = data;
	int i;

	for (i = 0; uris[i] != NULL; i++) {
		GFile *file;

		file = g_file_new_for_uri (uris[i]);

		nemo_application_open_location (fdb->application, file, NULL, startup_id);

		g_object_unref (file);
	}

	nemo_freedesktop_nemo_file_manager1_complete_show_folders (object, invocation);
	return TRUE;
}

static gboolean
skeleton_handle_show_item_properties_cb (NemoFreedesktopNemoFileManager1 *object,
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

	nemo_freedesktop_nemo_file_manager1_complete_show_item_properties (object, invocation);
	return TRUE;
}

static gboolean
service_timeout_cb (gpointer data)
{
	NemoFreedesktopDBus *fdb = data;

	DEBUG ("Reached the DBus service timeout");

	/* just unconditionally release here, as if an operation has been
	 * called, its progress handler will hold it alive for all the task duration.
	 */
	g_application_release (G_APPLICATION (fdb->application));

	return FALSE;
}

static void
bus_acquired_cb (GDBusConnection *conn,
		 const gchar     *name,
		 gpointer         user_data)
{
	NemoFreedesktopDBus *fdb = user_data;

	DEBUG ("Bus acquired at %s", name);

	fdb->connection = g_object_ref (conn);
	fdb->object_manager = g_dbus_object_manager_server_new ("/org/freedesktop/NemoFileManager1");

	fdb->skeleton = nemo_freedesktop_nemo_file_manager1_skeleton_new ();

	g_signal_connect (fdb->skeleton, "handle-show-items",
			  G_CALLBACK (skeleton_handle_show_items_cb), fdb);
	g_signal_connect (fdb->skeleton, "handle-show-folders",
			  G_CALLBACK (skeleton_handle_show_folders_cb), fdb);
	g_signal_connect (fdb->skeleton, "handle-show-item-properties",
			  G_CALLBACK (skeleton_handle_show_item_properties_cb), fdb);

	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (fdb->skeleton), fdb->connection, "/org/freedesktop/NemoFileManager1", NULL);

	g_dbus_object_manager_server_set_connection (fdb->object_manager, fdb->connection);

	g_timeout_add_seconds (SERVICE_TIMEOUT, service_timeout_cb, fdb);
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
	g_clear_object (&fdb->connection);
	fdb->application = NULL;

	G_OBJECT_CLASS (nemo_freedesktop_dbus_parent_class)->dispose (object);
}

static void
nemo_freedesktop_dbus_constructed (GObject *object)
{
	NemoFreedesktopDBus *fdb = (NemoFreedesktopDBus *) object;

	G_OBJECT_CLASS (nemo_freedesktop_dbus_parent_class)->constructed (object);

	g_application_hold (G_APPLICATION (fdb->application));

	fdb->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					"org.freedesktop.NemoFileManager1",
					G_BUS_NAME_OWNER_FLAGS_NONE,
					bus_acquired_cb,
					name_acquired_cb,
					name_lost_cb,
					fdb,
					NULL);
}

static void
nemo_freedesktop_dbus_set_property (GObject *object,
					guint property_id,
					const GValue *value,
					GParamSpec *pspec)
{
	NemoFreedesktopDBus *fdb = (NemoFreedesktopDBus *) object;

	switch (property_id) {
	case PROP_APPLICATION:
		fdb->application = g_value_get_object (value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}


static void
nemo_freedesktop_dbus_class_init (NemoFreedesktopDBusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose		= nemo_freedesktop_dbus_dispose;
	object_class->constructed	= nemo_freedesktop_dbus_constructed;
	object_class->set_property	= nemo_freedesktop_dbus_set_property;

	g_object_class_install_property (object_class,
					 PROP_APPLICATION,
					 g_param_spec_object ("application",
							      "NemoApplication instance",
							      "The owning NemoApplication instance",
							      NEMO_TYPE_APPLICATION,
							      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

}

static void
nemo_freedesktop_dbus_init (NemoFreedesktopDBus *fdb)
{
	/* nothing */
}

/* Tries to own the org.freedesktop.NemoFileManager1 service name */
void
nemo_freedesktop_dbus_start (NemoApplication *app)
{	
	if (singleton != NULL) {
		return;
	}

	singleton = g_object_new (nemo_freedesktop_dbus_get_type (),
				  "application", app,
				  NULL);
}

/* Releases the org.freedesktop.NemoFileManager1 service name */
void
nemo_freedesktop_dbus_stop (void)
{
	g_clear_object (&singleton);
}
