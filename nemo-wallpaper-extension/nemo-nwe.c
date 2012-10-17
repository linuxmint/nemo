/*
 *  Nemo Wallpaper extension
 *
 *  Copyright (C) 2005 Adam Israel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Author: Adam Israel <adam@battleaxe.net> 
 */
 
#include <config.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <libnemo-extension/nemo-extension-types.h>
#include <libnemo-extension/nemo-file-info.h>
#include <libnemo-extension/nemo-menu-provider.h>
#include <libnemo-private/nemo-global-preferences.h>
#include "nemo-nwe.h"

static GObjectClass *parent_class;

static void
set_wallpaper_callback (NemoMenuItem *item,
              gpointer          user_data)
{
    GList            *files;
    GError *err;
    NemoFileInfo *file;
    gchar            *uri;

    files = g_object_get_data (G_OBJECT (item), "files");
    file = files->data;

    uri = nemo_file_info_get_uri (file);

    g_settings_set_string (gnome_background_preferences,
                                    "picture-uri", uri);
    g_free (uri);
}

static gboolean
is_image (NemoFileInfo *file)
{
    gchar *mimeType;
    gboolean isImage;
    
    mimeType = nemo_file_info_get_mime_type (file);
    
    isImage = g_str_has_prefix (nemo_file_info_get_mime_type (file), "image/");
    
    g_free (mimeType);
    
    return isImage; 
}


static GList *
nemo_nwe_get_file_items (NemoMenuProvider *provider,
                  GtkWidget            *window,
                  GList                *files)
{
    GList    *items = NULL;
    GList    *scan;
    gboolean  one_item;
    NemoMenuItem *item;

    
    for (scan = files; scan; scan = scan->next) {
        NemoFileInfo *file = scan->data;
        gchar            *scheme;
        gboolean          local;

        scheme = nemo_file_info_get_uri_scheme (file);
        local = strncmp (scheme, "file", 4) == 0;
        g_free (scheme);

        if (!local)
            return NULL;
    }
    
    one_item = (files != NULL) && (files->next == NULL);
    if (one_item && is_image ((NemoFileInfo *)files->data) &&
        !nemo_file_info_is_directory ((NemoFileInfo *)files->data)) {
        item = nemo_menu_item_new ("NemoNwe::sendto",
                           _("Set as Wallpaper..."),
                           _("Set image as the current wallpaper..."),
                           NULL);
        g_signal_connect (item, 
                  "activate",
                  G_CALLBACK (set_wallpaper_callback),
                provider);
        g_object_set_data_full (G_OBJECT (item), 
                    "files",
                    nemo_file_info_list_copy (files),
                    (GDestroyNotify) nemo_file_info_list_free);
        items = g_list_append (items, item);
    }
    return items;
}


static void 
nemo_nwe_menu_provider_iface_init (NemoMenuProviderIface *iface)
{
    iface->get_file_items = nemo_nwe_get_file_items;
}


static void 
nemo_nwe_instance_init (NemoNwe *nwe)
{
}


static void
nemo_nwe_class_init (NemoNweClass *class)
{
    parent_class = g_type_class_peek_parent (class);
}


static GType nwe_type = 0;


GType
nemo_nwe_get_type (void) 
{
    return nwe_type;
}


void
nemo_nwe_register_type (GTypeModule *module)
{
    static const GTypeInfo info = {
        sizeof (NemoNweClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) nemo_nwe_class_init,
        NULL, 
        NULL,
        sizeof (NemoNwe),
        0,
        (GInstanceInitFunc) nemo_nwe_instance_init,
    };

    static const GInterfaceInfo menu_provider_iface_info = {
        (GInterfaceInitFunc) nemo_nwe_menu_provider_iface_init,
        NULL,
        NULL
    };

    nwe_type = g_type_module_register_type (module,
                             G_TYPE_OBJECT,
                             "NemoNwe",
                             &info, 0);

    g_type_module_add_interface (module,
                     nwe_type,
                     NEMO_TYPE_MENU_PROVIDER,
                     &menu_provider_iface_info);
}
