/*
 *  nemo-file-info.h - Information about a file 
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

/* NemoFileInfo is an interface to the NemoFile object.  It 
 * provides access to the asynchronous data in the NemoFile.
 * Extensions are passed objects of this type for operations. */

#ifndef NEMO_FILE_INFO_H
#define NEMO_FILE_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NEMO_TYPE_FILE_INFO           (nemo_file_info_get_type ())
#define NEMO_FILE_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_FILE_INFO, NemoFileInfo))
#define NEMO_IS_FILE_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_FILE_INFO))
#define NEMO_FILE_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NEMO_TYPE_FILE_INFO, NemoFileInfoIface))


#ifndef NEMO_FILE_DEFINED
#define NEMO_FILE_DEFINED
/* Using NemoFile for the vtable to make implementing this in 
 * NemoFile easier */
typedef struct NemoFile          NemoFile;
#endif

typedef NemoFile                  NemoFileInfo;
typedef struct _NemoFileInfoIface NemoFileInfoIface;


struct _NemoFileInfoIface 
{
	GTypeInterface g_iface;

	gboolean          (*is_gone)              (NemoFileInfo *file);
	
	char *            (*get_name)             (NemoFileInfo *file);
	char *            (*get_uri)              (NemoFileInfo *file);
	char *            (*get_parent_uri)       (NemoFileInfo *file);
	char *            (*get_uri_scheme)       (NemoFileInfo *file);
	
	char *            (*get_mime_type)        (NemoFileInfo *file);
	gboolean          (*is_mime_type)         (NemoFileInfo *file,
						   const char       *mime_Type);
	gboolean          (*is_directory)         (NemoFileInfo *file);
	
	void              (*add_emblem)           (NemoFileInfo *file,
						   const char       *emblem_name);
	char *            (*get_string_attribute) (NemoFileInfo *file,
						   const char       *attribute_name);
	void              (*add_string_attribute) (NemoFileInfo *file,
						   const char       *attribute_name,
						   const char       *value);
	void              (*invalidate_extension_info) (NemoFileInfo *file);
	
	char *            (*get_activation_uri)   (NemoFileInfo *file);

	GFileType         (*get_file_type)        (NemoFileInfo *file);
	GFile *           (*get_location)         (NemoFileInfo *file);
	GFile *           (*get_parent_location)  (NemoFileInfo *file);
	NemoFileInfo* (*get_parent_info)      (NemoFileInfo *file);
	GMount *          (*get_mount)            (NemoFileInfo *file);
	gboolean          (*can_write)            (NemoFileInfo *file);
  
};

GList            *nemo_file_info_list_copy            (GList            *files);
void              nemo_file_info_list_free            (GList            *files);
GType             nemo_file_info_get_type             (void);

/* Return true if the file has been deleted */
gboolean          nemo_file_info_is_gone              (NemoFileInfo *file);

/* Name and Location */
GFileType         nemo_file_info_get_file_type        (NemoFileInfo *file);
GFile *           nemo_file_info_get_location         (NemoFileInfo *file);
char *            nemo_file_info_get_name             (NemoFileInfo *file);
char *            nemo_file_info_get_uri              (NemoFileInfo *file);
char *            nemo_file_info_get_activation_uri   (NemoFileInfo *file);
GFile *           nemo_file_info_get_parent_location  (NemoFileInfo *file);
char *            nemo_file_info_get_parent_uri       (NemoFileInfo *file);
GMount *          nemo_file_info_get_mount            (NemoFileInfo *file);
char *            nemo_file_info_get_uri_scheme       (NemoFileInfo *file);
/* It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nemo.
 */
NemoFileInfo* nemo_file_info_get_parent_info      (NemoFileInfo *file);

/* File Type */
char *            nemo_file_info_get_mime_type        (NemoFileInfo *file);
gboolean          nemo_file_info_is_mime_type         (NemoFileInfo *file,
							   const char       *mime_type);
gboolean          nemo_file_info_is_directory         (NemoFileInfo *file);
gboolean          nemo_file_info_can_write            (NemoFileInfo *file);


/* Modifying the NemoFileInfo */
void              nemo_file_info_add_emblem           (NemoFileInfo *file,
							   const char       *emblem_name);
char *            nemo_file_info_get_string_attribute (NemoFileInfo *file,
							   const char       *attribute_name);
void              nemo_file_info_add_string_attribute (NemoFileInfo *file,
							   const char       *attribute_name,
							   const char       *value);

/* Invalidating file info */
void              nemo_file_info_invalidate_extension_info (NemoFileInfo *file);

NemoFileInfo *nemo_file_info_lookup                (GFile *location);
NemoFileInfo *nemo_file_info_create                (GFile *location);
NemoFileInfo *nemo_file_info_lookup_for_uri        (const char *uri);
NemoFileInfo *nemo_file_info_create_for_uri        (const char *uri);

G_END_DECLS

#endif
