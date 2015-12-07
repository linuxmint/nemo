/*
 *  nemo-module.h - Interface to nemo extensions
 * 
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Dave Camp <dave@ximian.com>
 * 
 */

#ifndef NEMO_MODULE_H
#define NEMO_MODULE_H

#include <glib-object.h>
#include <gmodule.h>


G_BEGIN_DECLS

#define NEMO_TYPE_MODULE        (nemo_module_get_type ())
#define NEMO_MODULE(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MODULE, NemoModule))
#define NEMO_MODULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_MODULE, NemoModule))
#define NEMO_IS_MODULE(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MODULE))
#define NEMO_IS_MODULE_CLASS(klass) (G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_MODULE))

typedef struct _NemoModule        NemoModule;
typedef struct _NemoModuleClass   NemoModuleClass;

struct _NemoModule {
    GTypeModule parent;

    GModule *library;

    char *path;

    void (*initialize) (GTypeModule  *module);
    void (*shutdown)   (void);

    void (*list_types) (const GType **types,
                int          *num_types);
    void (*get_modules_name_and_desc) (gchar ***strings);
};

struct _NemoModuleClass {
    GTypeModuleClass parent;
};

GType nemo_module_get_type (void);

void   nemo_module_setup                   (void);
void   nemo_module_refresh                 (void);
GList *nemo_module_get_extensions_for_type (GType  type);
void   nemo_module_extension_list_free     (GList *list);

/* Add a type to the module interface - allows nemo to add its own modules
 * without putting them in separate shared libraries */
void   nemo_module_add_type                (GType  type);

G_END_DECLS

#endif
