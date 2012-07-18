/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Dave Camp <dave@ximian.com>
 * 
 */

#include <config.h>
#include "nemo-module.h"

#include <eel/eel-debug.h>
#include <gmodule.h>

#define NEMO_TYPE_MODULE    	(nemo_module_get_type ())
#define NEMO_MODULE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_MODULE, NemoModule))
#define NEMO_MODULE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_MODULE, NemoModule))
#define NEMO_IS_MODULE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_MODULE))
#define NEMO_IS_MODULE_CLASS(klass)	(G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_MODULE))

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

};

struct _NemoModuleClass {
	GTypeModuleClass parent;	
};

static GList *module_objects = NULL;

static GType nemo_module_get_type (void);

G_DEFINE_TYPE (NemoModule, nemo_module, G_TYPE_TYPE_MODULE);

static gboolean
module_pulls_in_orbit (GModule *module)
{
	gpointer symbol;
	gboolean res;

	res = g_module_symbol (module, "ORBit_realloc_tcval", &symbol);

	return res;
}

static gboolean
nemo_module_load (GTypeModule *gmodule)
{
	NemoModule *module;
	
	module = NEMO_MODULE (gmodule);
	
	module->library = g_module_open (module->path, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

	if (!module->library) {
		g_warning ("%s", g_module_error ());
		return FALSE;
	}

	/* ORBit installs atexit() handlers, which would get unloaded together
	 * with the module now that the main process doesn't depend on GConf anymore,
	 * causing nemo to sefgault at exit.
	 * If we detect that an extension would pull in ORBit, we make the
	 * module resident to prevent that.
	 */
        if (module_pulls_in_orbit (module->library)) {
		g_module_make_resident (module->library);
        }

	if (!g_module_symbol (module->library,
			      "nemo_module_initialize",
			      (gpointer *)&module->initialize) ||
	    !g_module_symbol (module->library,
			      "nemo_module_shutdown",
			      (gpointer *)&module->shutdown) ||
	    !g_module_symbol (module->library,
			      "nemo_module_list_types",
			      (gpointer *)&module->list_types)) {

		g_warning ("%s", g_module_error ());
		g_module_close (module->library);
		
		return FALSE;
	}

	module->initialize (gmodule);
	
	return TRUE;
}

static void
nemo_module_unload (GTypeModule *gmodule)
{
	NemoModule *module;
	
	module = NEMO_MODULE (gmodule);
	
	module->shutdown ();
	
	g_module_close (module->library);
	
	module->initialize = NULL;
	module->shutdown = NULL;
	module->list_types = NULL;
}

static void
nemo_module_finalize (GObject *object)
{
	NemoModule *module;
	
	module = NEMO_MODULE (object);

	g_free (module->path);
	
	G_OBJECT_CLASS (nemo_module_parent_class)->finalize (object);
}

static void
nemo_module_init (NemoModule *module)
{
}

static void
nemo_module_class_init (NemoModuleClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nemo_module_finalize;
	G_TYPE_MODULE_CLASS (class)->load = nemo_module_load;
	G_TYPE_MODULE_CLASS (class)->unload = nemo_module_unload;
}

static void
module_object_weak_notify (gpointer user_data, GObject *object)
{
	module_objects = g_list_remove (module_objects, object);
}

static void
add_module_objects (NemoModule *module)
{
	const GType *types;
	int num_types;
	int i;
	
	module->list_types (&types, &num_types);
	
	for (i = 0; i < num_types; i++) {
		if (types[i] == 0) { /* Work around broken extensions */
			break;
		}
		nemo_module_add_type (types[i]);
	}
}

static NemoModule *
nemo_module_load_file (const char *filename)
{
	NemoModule *module;
	
	module = g_object_new (NEMO_TYPE_MODULE, NULL);
	module->path = g_strdup (filename);
	
	if (g_type_module_use (G_TYPE_MODULE (module))) {
		add_module_objects (module);
		g_type_module_unuse (G_TYPE_MODULE (module));
		return module;
	} else {
		g_object_unref (module);
		return NULL;
	}
}

static void
load_module_dir (const char *dirname)
{
	GDir *dir;
	
	dir = g_dir_open (dirname, 0, NULL);
	
	if (dir) {
		const char *name;
		
		while ((name = g_dir_read_name (dir))) {
			if (g_str_has_suffix (name, "." G_MODULE_SUFFIX)) {
				char *filename;

				filename = g_build_filename (dirname, 
							     name, 
							     NULL);
				nemo_module_load_file (filename);
				g_free (filename);
			}
		}

		g_dir_close (dir);
	}
}

static void
free_module_objects (void)
{
	GList *l, *next;
	
	for (l = module_objects; l != NULL; l = next) {
		next = l->next;
		g_object_unref (l->data);
	}
	
	g_list_free (module_objects);
}

void
nemo_module_setup (void)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		initialized = TRUE;
		
		load_module_dir (NEMO_EXTENSIONDIR);

		eel_debug_call_at_shutdown (free_module_objects);
	}
}

GList *
nemo_module_get_extensions_for_type (GType type)
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

void
nemo_module_extension_list_free (GList *extensions)
{
	GList *l, *next;
	
	for (l = extensions; l != NULL; l = next) {
		next = l->next;
		g_object_unref (l->data);
	}
	g_list_free (extensions);
}

void   
nemo_module_add_type (GType type)
{
	GObject *object;
	
	object = g_object_new (type, NULL);
	g_object_weak_ref (object, 
			   (GWeakNotify)module_object_weak_notify,
			   NULL);

	module_objects = g_list_prepend (module_objects, object);
}
