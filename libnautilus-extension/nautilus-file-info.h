/*
 *  nautilus-file-info.h - Information about a file 
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
 */

/* NautilusFileInfo is an interface to the NautilusFile object.  It 
 * provides access to the asynchronous data in the NautilusFile.
 * Extensions are passed objects of this type for operations. */

#ifndef NAUTILUS_FILE_INFO_H
#define NAUTILUS_FILE_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FILE_INFO           (nautilus_file_info_get_type ())
#define NAUTILUS_FILE_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_FILE_INFO, NautilusFileInfo))
#define NAUTILUS_IS_FILE_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_FILE_INFO))
#define NAUTILUS_FILE_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_FILE_INFO, NautilusFileInfoIface))


#ifndef NAUTILUS_FILE_DEFINED
#define NAUTILUS_FILE_DEFINED
/* Using NautilusFile for the vtable to make implementing this in 
 * NautilusFile easier */
typedef struct NautilusFile          NautilusFile;
#endif

typedef NautilusFile                  NautilusFileInfo;
typedef struct _NautilusFileInfoIface NautilusFileInfoIface;


struct _NautilusFileInfoIface 
{
	GTypeInterface g_iface;

	gboolean          (*is_gone)              (NautilusFileInfo *file);
	
	char *            (*get_name)             (NautilusFileInfo *file);
	char *            (*get_uri)              (NautilusFileInfo *file);
	char *            (*get_parent_uri)       (NautilusFileInfo *file);
	char *            (*get_uri_scheme)       (NautilusFileInfo *file);
	
	char *            (*get_mime_type)        (NautilusFileInfo *file);
	gboolean          (*is_mime_type)         (NautilusFileInfo *file,
						   const char       *mime_Type);
	gboolean          (*is_directory)         (NautilusFileInfo *file);
	
	void              (*add_emblem)           (NautilusFileInfo *file,
						   const char       *emblem_name);
	char *            (*get_string_attribute) (NautilusFileInfo *file,
						   const char       *attribute_name);
	void              (*add_string_attribute) (NautilusFileInfo *file,
						   const char       *attribute_name,
						   const char       *value);
	void              (*invalidate_extension_info) (NautilusFileInfo *file);
	
	char *            (*get_activation_uri)   (NautilusFileInfo *file);

	GFileType         (*get_file_type)        (NautilusFileInfo *file);
	GFile *           (*get_location)         (NautilusFileInfo *file);
	GFile *           (*get_parent_location)  (NautilusFileInfo *file);
	NautilusFileInfo* (*get_parent_info)      (NautilusFileInfo *file);
	GMount *          (*get_mount)            (NautilusFileInfo *file);
	gboolean          (*can_write)            (NautilusFileInfo *file);
  
};

GList            *nautilus_file_info_list_copy            (GList            *files);
void              nautilus_file_info_list_free            (GList            *files);
GType             nautilus_file_info_get_type             (void);

/* Return true if the file has been deleted */
gboolean          nautilus_file_info_is_gone              (NautilusFileInfo *file);

/* Name and Location */
GFileType         nautilus_file_info_get_file_type        (NautilusFileInfo *file);
GFile *           nautilus_file_info_get_location         (NautilusFileInfo *file);
char *            nautilus_file_info_get_name             (NautilusFileInfo *file);
char *            nautilus_file_info_get_uri              (NautilusFileInfo *file);
char *            nautilus_file_info_get_activation_uri   (NautilusFileInfo *file);
GFile *           nautilus_file_info_get_parent_location  (NautilusFileInfo *file);
char *            nautilus_file_info_get_parent_uri       (NautilusFileInfo *file);
GMount *          nautilus_file_info_get_mount            (NautilusFileInfo *file);
char *            nautilus_file_info_get_uri_scheme       (NautilusFileInfo *file);
/* It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nautilus.
 */
NautilusFileInfo* nautilus_file_info_get_parent_info      (NautilusFileInfo *file);

/* File Type */
char *            nautilus_file_info_get_mime_type        (NautilusFileInfo *file);
gboolean          nautilus_file_info_is_mime_type         (NautilusFileInfo *file,
							   const char       *mime_type);
gboolean          nautilus_file_info_is_directory         (NautilusFileInfo *file);
gboolean          nautilus_file_info_can_write            (NautilusFileInfo *file);


/* Modifying the NautilusFileInfo */
void              nautilus_file_info_add_emblem           (NautilusFileInfo *file,
							   const char       *emblem_name);
char *            nautilus_file_info_get_string_attribute (NautilusFileInfo *file,
							   const char       *attribute_name);
void              nautilus_file_info_add_string_attribute (NautilusFileInfo *file,
							   const char       *attribute_name,
							   const char       *value);

/* Invalidating file info */
void              nautilus_file_info_invalidate_extension_info (NautilusFileInfo *file);

NautilusFileInfo *nautilus_file_info_lookup                (GFile *location);
NautilusFileInfo *nautilus_file_info_create                (GFile *location);
NautilusFileInfo *nautilus_file_info_lookup_for_uri        (const char *uri);
NautilusFileInfo *nautilus_file_info_create_for_uri        (const char *uri);

G_END_DECLS

#endif
