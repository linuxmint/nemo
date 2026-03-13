/*
 * nemo-dbus-manager: nemo DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nemo-dbus-manager.h"
#include "nemo-generated.h"

#include "nemo-file-operations.h"

#define DEBUG_FLAG NEMO_DEBUG_DBUS
#include "nemo-debug.h"

#include <gio/gio.h>

struct _NemoDBusManager {
  GObject parent;

  GDBusObjectManagerServer *object_manager;
  NemoDBusFileOperations *file_operations;
};

struct _NemoDBusManagerClass {
  GObjectClass parent_class;
};

G_DEFINE_TYPE (NemoDBusManager, nemo_dbus_manager, G_TYPE_OBJECT);

static void
nemo_dbus_manager_dispose (GObject *object)
{
  NemoDBusManager *self = (NemoDBusManager *) object;

  if (self->file_operations) {
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->file_operations));
    g_object_unref (self->file_operations);
    self->file_operations = NULL;
  }

  if (self->object_manager) {
    g_object_unref (self->object_manager);
    self->object_manager = NULL;
  }

  G_OBJECT_CLASS (nemo_dbus_manager_parent_class)->dispose (object);
}

static gboolean
handle_copy_file (NemoDBusFileOperations *object,
		  GDBusMethodInvocation *invocation,
		  const gchar *source_uri,
		  const gchar *source_display_name,
		  const gchar *dest_dir_uri,
		  const gchar *dest_name)
{
  GFile *source_file, *target_dir;
  const gchar *target_name = NULL, *source_name = NULL;

  source_file = g_file_new_for_uri (source_uri);
  target_dir = g_file_new_for_uri (dest_dir_uri);

  if (dest_name != NULL && dest_name[0] != '\0')
    target_name = dest_name;

  if (source_display_name != NULL && source_display_name[0] != '\0')
    source_name = source_display_name;

  nemo_file_operations_copy_file (source_file, target_dir, source_name, target_name,
				      NULL, NULL, NULL);

  g_object_unref (source_file);
  g_object_unref (target_dir);

  nemo_dbus_file_operations_complete_copy_file (object, invocation);
  return TRUE; /* invocation was handled */
}

static gboolean
selection_event (NemoDBusFileOperations *object,
		  GDBusMethodInvocation *invocation,
		  const guint16 direction)
{
  printf("%i", direction);
  return TRUE; /* invocation was handled */
}

static gboolean
handle_copy_uris (NemoDBusFileOperations *object,
		  GDBusMethodInvocation *invocation,
		  const gchar **sources,
		  const gchar *destination)
{
  GList *source_files = NULL;
  GFile *dest_dir;
  gint idx;

  dest_dir = g_file_new_for_uri (destination);

  for (idx = 0; sources[idx] != NULL; idx++)
    source_files = g_list_prepend (source_files,
                                   g_file_new_for_uri (sources[idx]));

  nemo_file_operations_copy (source_files, NULL,
                                 dest_dir,
                                 NULL, NULL, NULL);

  g_list_free_full (source_files, g_object_unref);
  g_object_unref (dest_dir);

  nemo_dbus_file_operations_complete_copy_uris (object, invocation);
  return TRUE; /* invocation was handled */
}

static gboolean
handle_move_uris (NemoDBusFileOperations *object,
		  GDBusMethodInvocation *invocation,
		  const gchar **sources,
		  const gchar *destination)
{
  GList *source_files = NULL;
  GFile *dest_dir;
  gint idx;

  dest_dir = g_file_new_for_uri (destination);

  for (idx = 0; sources[idx] != NULL; idx++)
    source_files = g_list_prepend (source_files,
                                   g_file_new_for_uri (sources[idx]));

  nemo_file_operations_move (source_files, NULL,
                                 dest_dir,
                                 NULL, NULL, NULL);

  g_list_free_full (source_files, g_object_unref);
  g_object_unref (dest_dir);

  nemo_dbus_file_operations_complete_move_uris (object, invocation);
  return TRUE; /* invocation was handled */
}

static gboolean
handle_empty_trash (NemoDBusFileOperations *object,
		    GDBusMethodInvocation *invocation)
{
  nemo_file_operations_empty_trash (NULL);

  nemo_dbus_file_operations_complete_empty_trash (object, invocation);
  return TRUE; /* invocation was handled */
}

static void
nemo_dbus_manager_init (NemoDBusManager *self)
{
  GDBusConnection *connection;

  connection = g_application_get_dbus_connection (g_application_get_default ());

  self->object_manager = g_dbus_object_manager_server_new ("/org/Nemo");
  self->file_operations = nemo_dbus_file_operations_skeleton_new ();

  g_signal_connect (self->file_operations,
		    "handle-copy-uris",
		    G_CALLBACK (handle_copy_uris),
		    self);
  g_signal_connect (self->file_operations,
		    "handle-copy-file",
		    G_CALLBACK (handle_copy_file),
		    self);
  g_signal_connect (self->file_operations,
		    "handle-move-uris",
		    G_CALLBACK (handle_move_uris),
		    self);
  g_signal_connect (self->file_operations,
		    "handle-empty-trash",
		    G_CALLBACK (handle_empty_trash),
		    self);
  g_signal_connect (self->file_operations,
		    "selection-event",
		    G_CALLBACK (selection_event),
		    self);

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->file_operations), connection,
				    "/org/Nemo", NULL);

  g_dbus_object_manager_server_set_connection (self->object_manager, connection);
}

static void
nemo_dbus_manager_class_init (NemoDBusManagerClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = nemo_dbus_manager_dispose;
}

NemoDBusManager *
nemo_dbus_manager_new (void)
{
  return g_object_new (nemo_dbus_manager_get_type (),
                       NULL);
}
