/*
 *  nemo-file-info.c - Information about a file 
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
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 */

#include <config.h>
#include "nemo-file-info.h"
#include "nemo-extension-private.h"

NemoFileInfo *(*nemo_file_info_getter) (GFile *location, gboolean create);

/**
 * nemo_file_info_list_copy:
 * @files: (element-type NemoFileInfo): the files to copy
 *
 * Returns: (element-type NemoFileInfo) (transfer full): a copy of @files.
 *  Use #nemo_file_info_list_free to free the list and unref its contents.
 */
GList *
nemo_file_info_list_copy (GList *files)
{
	GList *ret;
	GList *l;
	
	ret = g_list_copy (files);
	for (l = ret; l != NULL; l = l->next) {
		g_object_ref (G_OBJECT (l->data));
	}

	return ret;
}

/**
 * nemo_file_info_list_free:
 * @files: (element-type NemoFileInfo): a list created with
 *   #nemo_file_info_list_copy
 *
 */
void              
nemo_file_info_list_free (GList *files)
{
	GList *l;
	
	for (l = files; l != NULL; l = l->next) {
		g_object_unref (G_OBJECT (l->data));
	}
	
	g_list_free (files);
}

static void
nemo_file_info_base_init (gpointer g_class)
{
}

GType                   
nemo_file_info_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NemoFileInfoIface),
			nemo_file_info_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NemoFileInfo",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

gboolean
nemo_file_info_is_gone (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->is_gone != NULL, FALSE);
	
	return NEMO_FILE_INFO_GET_IFACE (file)->is_gone (file);
}

GFileType
nemo_file_info_get_file_type (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), G_FILE_TYPE_UNKNOWN);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_file_type != NULL, G_FILE_TYPE_UNKNOWN);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_file_type (file);
}

char *
nemo_file_info_get_name (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_name != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_name (file);
}

/**
 * nemo_file_info_get_location:
 * @file: a #NemoFileInfo
 *
 * Returns: (transfer full): a #GFile for the location of @file
 */
GFile *
nemo_file_info_get_location (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_location != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_location (file);
}
char *
nemo_file_info_get_uri (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_uri != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_uri (file);
}

char *
nemo_file_info_get_activation_uri (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_activation_uri != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_activation_uri (file);
}

/**
 * nemo_file_info_get_parent_location:
 * @file: a #NemoFileInfo
 *
 * Returns: (allow-none) (transfer full): a #GFile for the parent location of @file, 
 *   or %NULL if @file has no parent
 */
GFile *
nemo_file_info_get_parent_location (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_parent_location != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_parent_location (file);
}

char *
nemo_file_info_get_parent_uri (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_parent_uri != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_parent_uri (file);
}

/**
 * nemo_file_info_get_parent_info:
 * @file: a #NemoFileInfo
 *
 * Returns: (allow-none) (transfer full): a #NemoFileInfo for the parent of @file, 
 *   or %NULL if @file has no parent
 */
NemoFileInfo *
nemo_file_info_get_parent_info (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_parent_info != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_parent_info (file);
}

/**
 * nemo_file_info_get_mount:
 * @file: a #NemoFileInfo
 *
 * Returns: (allow-none) (transfer full): a #GMount for the mount of @file, 
 *   or %NULL if @file has no mount
 */
GMount *
nemo_file_info_get_mount (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_mount != NULL, NULL);
    
	return NEMO_FILE_INFO_GET_IFACE (file)->get_mount (file);
}

char *
nemo_file_info_get_uri_scheme (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_uri_scheme != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_uri_scheme (file);
}

char *
nemo_file_info_get_mime_type (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_mime_type != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_mime_type (file);
}

gboolean
nemo_file_info_is_mime_type (NemoFileInfo *file,
				 const char *mime_type)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->is_mime_type != NULL, FALSE);

	return NEMO_FILE_INFO_GET_IFACE (file)->is_mime_type (file,
								  mime_type);
}

gboolean
nemo_file_info_is_directory (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->is_directory != NULL, FALSE);

	return NEMO_FILE_INFO_GET_IFACE (file)->is_directory (file);
}

gboolean
nemo_file_info_can_write (NemoFileInfo *file)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->can_write != NULL, FALSE);

	return NEMO_FILE_INFO_GET_IFACE (file)->can_write (file);
}

void
nemo_file_info_add_emblem (NemoFileInfo *file,
			       const char *emblem_name)
{
	g_return_if_fail (NEMO_IS_FILE_INFO (file));
	g_return_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->add_emblem != NULL);

	NEMO_FILE_INFO_GET_IFACE (file)->add_emblem (file, emblem_name);
}

char *
nemo_file_info_get_string_attribute (NemoFileInfo *file,
					 const char *attribute_name)
{
	g_return_val_if_fail (NEMO_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->get_string_attribute != NULL, NULL);
	g_return_val_if_fail (attribute_name != NULL, NULL);

	return NEMO_FILE_INFO_GET_IFACE (file)->get_string_attribute 
		(file, attribute_name);
}

void
nemo_file_info_add_string_attribute (NemoFileInfo *file,
					 const char *attribute_name,
					 const char *value)
{
	g_return_if_fail (NEMO_IS_FILE_INFO (file));
	g_return_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->add_string_attribute != NULL);
	g_return_if_fail (attribute_name != NULL);
	g_return_if_fail (value != NULL);
	
	NEMO_FILE_INFO_GET_IFACE (file)->add_string_attribute 
		(file, attribute_name, value);
}

void
nemo_file_info_invalidate_extension_info (NemoFileInfo *file)
{
	g_return_if_fail (NEMO_IS_FILE_INFO (file));
	g_return_if_fail (NEMO_FILE_INFO_GET_IFACE (file)->invalidate_extension_info != NULL);
	
	NEMO_FILE_INFO_GET_IFACE (file)->invalidate_extension_info (file);
}

/**
 * nemo_file_info_lookup:
 * @location: the location to lookup the file info for
 *
 * Returns: (transfer full): a #NemoFileInfo
 */
NemoFileInfo *
nemo_file_info_lookup (GFile *location)
{
	return nemo_file_info_getter (location, FALSE);
}

/**
 * nemo_file_info_create:
 * @location: the location to create the file info for
 *
 * Returns: (transfer full): a #NemoFileInfo
 */
NemoFileInfo *
nemo_file_info_create (GFile *location)
{
	return nemo_file_info_getter (location, TRUE);
}

/**
 * nemo_file_info_lookup_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (transfer full): a #NemoFileInfo
 */
NemoFileInfo *
nemo_file_info_lookup_for_uri (const char *uri)
{
	GFile *location;
	NemoFile *file;

	location = g_file_new_for_uri (uri);
	file = nemo_file_info_lookup (location);
	g_object_unref (location);

	return file;
}

/**
 * nemo_file_info_create_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (transfer full): a #NemoFileInfo
 */
NemoFileInfo *
nemo_file_info_create_for_uri (const char *uri)
{
	GFile *location;
	NemoFile *file;

	location = g_file_new_for_uri (uri);
	file = nemo_file_info_create (location);
	g_object_unref (location);

	return file;
}
