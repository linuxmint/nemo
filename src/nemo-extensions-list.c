/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-open-with-main.c - Start the "Open with" dialog.
 * Nemo
 *
 * Copyright (C) 2005 Vincent Untz
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
 *
 * Authors:
 *   Vincent Untz <vincent@vuntz.net>
 *   Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <config.h>

#include <gmodule.h>
#include <libnemo-private/nemo-module.h>
#include <libnemo-extension/nemo-name-and-desc-provider.h>

#include <stdlib.h>

static GList *module_objects = NULL;

static void
module_object_weak_notify (gpointer user_data, GObject *object)
{
    module_objects = g_list_remove (module_objects, object);
}

static void
load_module_objects (NemoModule *module)
{
    const GType *types;
    int num_types;
    int i;

    module->list_types (&types, &num_types);

    for (i = 0; i < num_types; i++) {
        if (types[i] == 0) { /* Work around broken extensions */
            break;
        }

        GObject *object;
        
        object = g_object_new (types[i], NULL);
        g_object_weak_ref (object, 
                   (GWeakNotify)module_object_weak_notify,
                   NULL);

        module_objects = g_list_prepend (module_objects, object);
    }
}

static void
load_module_file (const char *filename)
{
    NemoModule *module = NULL;

    module = g_object_new (NEMO_TYPE_MODULE, NULL);
    module->path = g_strdup (filename);
    if (g_type_module_use (G_TYPE_MODULE (module))) {
        load_module_objects (module);
        g_type_module_unuse (G_TYPE_MODULE (module));
    } else {
        g_object_unref (module);
    }

}

static void
populate_from_directory (const gchar *path)
{
    GDir *dir;

    dir = g_dir_open (path, 0, NULL);

    if (dir) {
        const char *name;

        while ((name = g_dir_read_name (dir))) {
            if (g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
                char *filename;

                filename = g_build_filename (path, name, NULL);

                load_module_file (filename);

                g_free (filename);
            }
        }

        g_dir_close (dir);
    }
}

GList *
module_get_extensions_for_type (GType type)
{
    GList *l;
    GList *ret = NULL;
    
    for (l = module_objects; l != NULL; l = l->next) {
        if (G_TYPE_CHECK_INSTANCE_TYPE (G_OBJECT (l->data),
                        type)) {
            g_object_ref (l->data);
            ret = g_list_prepend (ret, l->data);
        }
    }

    return ret; 
}

int
main (int argc, char *argv[])
{
    gchar *path = NULL;

    path = g_build_filename ("/", "usr", "lib", "nemo", "extensions-3.0", NULL);
    populate_from_directory (path);
    g_clear_pointer (&path, g_free);

    GList *nd_providers;
    GList *l;

    nd_providers = module_get_extensions_for_type (NEMO_TYPE_NAME_AND_DESC_PROVIDER);

    for (l = module_objects; l != NULL; l = l->next) {
        GObject *obj = G_OBJECT (l->data);
        g_printf ("NEMO_EXTENSION:::%s", G_OBJECT_TYPE_NAME (obj));

        if (g_list_index (nd_providers, obj) > -1) {
            GList *nd_list = nemo_name_and_desc_provider_get_name_and_desc (NEMO_NAME_AND_DESC_PROVIDER (obj));

            g_printf (":::%s\n", (gchar *) nd_list->data);

            g_list_free (nd_list);
        } else {
            g_printf ("\n");
        }
    }

    g_list_free_full (nd_providers, (GDestroyNotify) g_object_unref);

    return 0;
}
