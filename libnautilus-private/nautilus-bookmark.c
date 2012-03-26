/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark.c - implementation of individual bookmarks.
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#include <config.h>

#include "nautilus-bookmark.h"

#include <eel/eel-vfs-extensions.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-names.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_BOOKMARKS
#include <libnautilus-private/nautilus-debug.h>

enum {
	CONTENTS_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_NAME = 1,
	PROP_CUSTOM_NAME,
	PROP_LOCATION,
	PROP_ICON,
	NUM_PROPERTIES
};

#define ELLIPSISED_MENU_ITEM_MIN_CHARS  32

static GParamSpec* properties[NUM_PROPERTIES] = { NULL };
static guint signals[LAST_SIGNAL];

struct NautilusBookmarkDetails
{
	char *name;
	gboolean has_custom_name;
	GFile *location;
	GIcon *icon;
	NautilusFile *file;
	
	char *scroll_file;
};

static void	  nautilus_bookmark_disconnect_file	  (NautilusBookmark	 *file);

G_DEFINE_TYPE (NautilusBookmark, nautilus_bookmark, G_TYPE_OBJECT);

static void
nautilus_bookmark_set_name_internal (NautilusBookmark *bookmark,
				     const char *new_name)
{
	if (g_strcmp0 (bookmark->details->name, new_name) != 0) {
		g_free (bookmark->details->name);
		bookmark->details->name = g_strdup (new_name);

		g_object_notify_by_pspec (G_OBJECT (bookmark), properties[PROP_NAME]);
	}
}

static void
nautilus_bookmark_update_icon (NautilusBookmark *bookmark)
{
	GIcon *new_icon;

	if (bookmark->details->file == NULL) {
		return;
	}

	if (!nautilus_file_is_local (bookmark->details->file)) {
		/* never update icons for remote bookmarks */
		return;
	}

	if (!nautilus_file_is_not_yet_confirmed (bookmark->details->file) &&
	    nautilus_file_check_if_ready (bookmark->details->file,
					  NAUTILUS_FILE_ATTRIBUTES_FOR_ICON)) {
		DEBUG ("%s: set new icon", nautilus_bookmark_get_name (bookmark));

		new_icon = nautilus_file_get_gicon (bookmark->details->file, 0);
		g_object_set (bookmark,
			      "icon", new_icon,
			      NULL);

		g_object_unref (new_icon);
	}
}

static void
bookmark_set_name_from_ready_file (NautilusBookmark *self,
				   NautilusFile *file)
{
	gchar *display_name;

	if (self->details->has_custom_name) {
		return;
	}

	display_name = nautilus_file_get_display_name (self->details->file);

	if (nautilus_file_is_home (self->details->file)) {
		nautilus_bookmark_set_name_internal (self, _("Home"));
	} else if (g_strcmp0 (self->details->name, display_name) != 0) {
		nautilus_bookmark_set_name_internal (self, display_name);
		DEBUG ("%s: name changed to %s", nautilus_bookmark_get_name (self), display_name);
	}

	g_free (display_name);
}

static void
bookmark_file_changed_callback (NautilusFile *file,
				NautilusBookmark *bookmark)
{
	GFile *location;

	g_assert (file == bookmark->details->file);

	DEBUG ("%s: file changed", nautilus_bookmark_get_name (bookmark));

	location = nautilus_file_get_location (file);

	if (!g_file_equal (bookmark->details->location, location) &&
	    !nautilus_file_is_in_trash (file)) {
		DEBUG ("%s: file got moved", nautilus_bookmark_get_name (bookmark));

		g_object_unref (bookmark->details->location);
		bookmark->details->location = g_object_ref (location);

		g_object_notify_by_pspec (G_OBJECT (bookmark), properties[PROP_LOCATION]);
		g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
	}

	g_object_unref (location);

	if (nautilus_file_is_gone (file) ||
	    nautilus_file_is_in_trash (file)) {
		/* The file we were monitoring has been trashed, deleted,
		 * or moved in a way that we didn't notice. We should make 
		 * a spanking new NautilusFile object for this 
		 * location so if a new file appears in this place 
		 * we will notice. However, we can't immediately do so
		 * because creating a new NautilusFile directly as a result
		 * of noticing a file goes away may trigger i/o on that file
		 * again, noticeing it is gone, leading to a loop.
		 * So, the new NautilusFile is created when the bookmark
		 * is used again. However, this is not really a problem, as
		 * we don't want to change the icon or anything about the
		 * bookmark just because its not there anymore.
		 */
		DEBUG ("%s: trashed", nautilus_bookmark_get_name (bookmark));
		nautilus_bookmark_disconnect_file (bookmark);
	} else {
		nautilus_bookmark_update_icon (bookmark);
		bookmark_set_name_from_ready_file (bookmark, file);
	}
}

static void
nautilus_bookmark_set_icon_to_default (NautilusBookmark *bookmark)
{
	GIcon *icon, *emblemed_icon, *folder;
	GEmblem *emblem;
	char *uri;

	if (g_file_is_native (bookmark->details->location)) {
		folder = g_themed_icon_new (NAUTILUS_ICON_FOLDER);
	} else {
		uri = nautilus_bookmark_get_uri (bookmark);
		if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
			folder = g_themed_icon_new (NAUTILUS_ICON_FOLDER_SAVED_SEARCH);
		} else {
			folder = g_themed_icon_new (NAUTILUS_ICON_FOLDER_REMOTE);
		}
		g_free (uri);
	}

	if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
		DEBUG ("%s: file does not exist, add emblem", nautilus_bookmark_get_name (bookmark));

		icon = g_themed_icon_new (GTK_STOCK_DIALOG_WARNING);
		emblem = g_emblem_new (icon);

		emblemed_icon = g_emblemed_icon_new (folder, emblem);

		g_object_unref (emblem);
		g_object_unref (icon);
		g_object_unref (folder);

		folder = emblemed_icon;
	}

	DEBUG ("%s: setting icon to default", nautilus_bookmark_get_name (bookmark));

	g_object_set (bookmark,
		      "icon", folder,
		      NULL);

	g_object_unref (folder);
}

static void
nautilus_bookmark_disconnect_file (NautilusBookmark *bookmark)
{
	if (bookmark->details->file != NULL) {
		DEBUG ("%s: disconnecting file",
		       nautilus_bookmark_get_name (bookmark));

		g_signal_handlers_disconnect_by_func (bookmark->details->file,
						      G_CALLBACK (bookmark_file_changed_callback),
						      bookmark);
		g_clear_object (&bookmark->details->file);
	}
}

static void
nautilus_bookmark_connect_file (NautilusBookmark *bookmark)
{
	if (bookmark->details->file != NULL) {
		DEBUG ("%s: file already connected, returning",
		       nautilus_bookmark_get_name (bookmark));
		return;
	}

	if (!nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
		DEBUG ("%s: creating file", nautilus_bookmark_get_name (bookmark));

		bookmark->details->file = nautilus_file_get (bookmark->details->location);
		g_assert (!nautilus_file_is_gone (bookmark->details->file));

		g_signal_connect_object (bookmark->details->file, "changed",
					 G_CALLBACK (bookmark_file_changed_callback), bookmark, 0);
	}

	/* Set icon based on available information. */
	nautilus_bookmark_update_icon (bookmark);

	if (bookmark->details->icon == NULL) {
		nautilus_bookmark_set_icon_to_default (bookmark);
	}

	if (bookmark->details->file != NULL &&
	    nautilus_file_check_if_ready (bookmark->details->file, NAUTILUS_FILE_ATTRIBUTE_INFO)) {
		bookmark_set_name_from_ready_file (bookmark, bookmark->details->file);
	}

	if (bookmark->details->name == NULL) {
		bookmark->details->name = nautilus_compute_title_for_location (bookmark->details->location);
	}
}

/* GObject methods */

static void
nautilus_bookmark_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	NautilusBookmark *self = NAUTILUS_BOOKMARK (object);
	GIcon *new_icon;

	switch (property_id) {
	case PROP_ICON:
		new_icon = g_value_get_object (value);

		if (new_icon != NULL && !g_icon_equal (self->details->icon, new_icon)) {
			g_clear_object (&self->details->icon);
			self->details->icon = g_object_ref (new_icon);
		}

		break;
	case PROP_LOCATION:
		self->details->location = g_value_dup_object (value);
		break;
	case PROP_CUSTOM_NAME:
		self->details->has_custom_name = g_value_get_boolean (value);
		break;
	case PROP_NAME:
		nautilus_bookmark_set_name_internal (self, g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_bookmark_get_property (GObject *object,
				guint property_id,
				GValue *value,
				GParamSpec *pspec)
{
	NautilusBookmark *self = NAUTILUS_BOOKMARK (object);

	switch (property_id) {
	case PROP_NAME:
		g_value_set_string (value, self->details->name);
		break;
	case PROP_ICON:
		g_value_set_object (value, self->details->icon);
		break;
	case PROP_LOCATION:
		g_value_set_object (value, self->details->location);
		break;
	case PROP_CUSTOM_NAME:
		g_value_set_boolean (value, self->details->has_custom_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_bookmark_finalize (GObject *object)
{
	NautilusBookmark *bookmark;

	g_assert (NAUTILUS_IS_BOOKMARK (object));

	bookmark = NAUTILUS_BOOKMARK (object);

	nautilus_bookmark_disconnect_file (bookmark);	

	g_object_unref (bookmark->details->location);
	g_clear_object (&bookmark->details->icon);

	g_free (bookmark->details->name);
	g_free (bookmark->details->scroll_file);

	G_OBJECT_CLASS (nautilus_bookmark_parent_class)->finalize (object);
}

static void
nautilus_bookmark_constructed (GObject *obj)
{
	NautilusBookmark *self = NAUTILUS_BOOKMARK (obj);

	nautilus_bookmark_connect_file (self);
}

static void
nautilus_bookmark_class_init (NautilusBookmarkClass *class)
{
	GObjectClass *oclass = G_OBJECT_CLASS (class);

	oclass->finalize = nautilus_bookmark_finalize;
	oclass->get_property = nautilus_bookmark_get_property;
	oclass->set_property = nautilus_bookmark_set_property;
	oclass->constructed = nautilus_bookmark_constructed;

	signals[CONTENTS_CHANGED] =
		g_signal_new ("contents-changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusBookmarkClass, contents_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	properties[PROP_NAME] =
		g_param_spec_string ("name",
				     "Bookmark's name",
				     "The name of this bookmark",
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

	properties[PROP_CUSTOM_NAME] =
		g_param_spec_boolean ("custom-name",
				      "Whether the bookmark has a custom name",
				      "Whether the bookmark has a custom name",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT);

	properties[PROP_LOCATION] =
		g_param_spec_object ("location",
				     "Bookmark's location",
				     "The location of this bookmark",
				     G_TYPE_FILE,
				     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT_ONLY);

	properties[PROP_ICON] =
		g_param_spec_object ("icon",
				     "Bookmark's icon",
				     "The icon of this bookmark",
				     G_TYPE_ICON,
				     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);

	g_type_class_add_private (class, sizeof (NautilusBookmarkDetails));
}

static void
nautilus_bookmark_init (NautilusBookmark *bookmark)
{
	bookmark->details = G_TYPE_INSTANCE_GET_PRIVATE (bookmark, NAUTILUS_TYPE_BOOKMARK,
							 NautilusBookmarkDetails);
}

const gchar *
nautilus_bookmark_get_name (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return bookmark->details->name;
}

gboolean
nautilus_bookmark_get_has_custom_name (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

	return (bookmark->details->has_custom_name);
}

/**
 * nautilus_bookmark_set_custom_name:
 *
 * Change the user-displayed name of a bookmark.
 * @new_name: The new user-displayed name for this bookmark, mustn't be NULL.
 *
 **/
void
nautilus_bookmark_set_custom_name (NautilusBookmark *bookmark,
				   const char *new_name)
{
	g_return_if_fail (new_name != NULL);
	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	g_object_set (bookmark,
		      "custom-name", TRUE,
		      "name", new_name,
		      NULL);

	g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
}

/**
 * nautilus_bookmark_compare_with:
 *
 * Check whether two bookmarks are considered identical.
 * @a: first NautilusBookmark*.
 * @b: second NautilusBookmark*.
 * 
 * Return value: 0 if @a and @b have same name and uri, 1 otherwise 
 * (GCompareFunc style)
 **/
int		    
nautilus_bookmark_compare_with (gconstpointer a, gconstpointer b)
{
	NautilusBookmark *bookmark_a;
	NautilusBookmark *bookmark_b;

	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (a), 1);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (b), 1);

	bookmark_a = NAUTILUS_BOOKMARK (a);
	bookmark_b = NAUTILUS_BOOKMARK (b);

	if (!g_file_equal (bookmark_a->details->location,
			   bookmark_b->details->location)) {
		return 1;
	}
	
	if (g_strcmp0 (bookmark_a->details->name,
		       bookmark_b->details->name) != 0) {
		return 1;
	}
	
	return 0;
}

/**
 * nautilus_bookmark_compare_uris:
 *
 * Check whether the uris of two bookmarks are for the same location.
 * @a: first NautilusBookmark*.
 * @b: second NautilusBookmark*.
 * 
 * Return value: 0 if @a and @b have matching uri, 1 otherwise 
 * (GCompareFunc style)
 **/
int		    
nautilus_bookmark_compare_uris (gconstpointer a, gconstpointer b)
{
	NautilusBookmark *bookmark_a;
	NautilusBookmark *bookmark_b;

	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (a), 1);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (b), 1);

	bookmark_a = NAUTILUS_BOOKMARK (a);
	bookmark_b = NAUTILUS_BOOKMARK (b);

	return !g_file_equal (bookmark_a->details->location,
			      bookmark_b->details->location);
}

NautilusBookmark *
nautilus_bookmark_copy (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return nautilus_bookmark_new (
			bookmark->details->location,
			bookmark->details->has_custom_name ?
			bookmark->details->name : NULL,
			bookmark->details->icon);
}

GIcon *
nautilus_bookmark_get_icon (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	/* Try to connect a file in case file exists now but didn't earlier. */
	nautilus_bookmark_connect_file (bookmark);

	if (bookmark->details->icon) {
		return g_object_ref (bookmark->details->icon);
	}
	return NULL;
}

GFile *
nautilus_bookmark_get_location (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	/* Try to connect a file in case file exists now but didn't earlier.
	 * This allows a bookmark to update its image properly in the case
	 * where a new file appears with the same URI as a previously-deleted
	 * file. Calling connect_file here means that attempts to activate the 
	 * bookmark will update its image if possible. 
	 */
	nautilus_bookmark_connect_file (bookmark);

	return g_object_ref (bookmark->details->location);
}

char *
nautilus_bookmark_get_uri (NautilusBookmark *bookmark)
{
	GFile *file;
	char *uri;

	file = nautilus_bookmark_get_location (bookmark);
	uri = g_file_get_uri (file);
	g_object_unref (file);
	return uri;
}

NautilusBookmark *
nautilus_bookmark_new (GFile *location,
		       const gchar *custom_name,
                       GIcon *icon)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = NAUTILUS_BOOKMARK (g_object_new (NAUTILUS_TYPE_BOOKMARK,
							"location", location,
							"icon", icon,
							"name", custom_name,
							"custom-name", custom_name != NULL,
							NULL));

	return new_bookmark;
}				 

static GtkWidget *
create_image_widget_for_bookmark (NautilusBookmark *bookmark)
{
	GIcon *icon;
	GtkWidget *widget;

	icon = nautilus_bookmark_get_icon (bookmark);
        widget = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);
	g_object_unref (icon);

	return widget;
}

/**
 * nautilus_bookmark_menu_item_new:
 * 
 * Return a menu item representing a bookmark.
 * @bookmark: The bookmark the menu item represents.
 * Return value: A newly-created bookmark, not yet shown.
 **/ 
GtkWidget *
nautilus_bookmark_menu_item_new (NautilusBookmark *bookmark)
{
	GtkWidget *menu_item;
	GtkWidget *image_widget;
	GtkLabel *label;
	const char *name;

	name = nautilus_bookmark_get_name (bookmark);
	menu_item = gtk_image_menu_item_new_with_label (name);
	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (menu_item)));
	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, ELLIPSISED_MENU_ITEM_MIN_CHARS);

	image_widget = create_image_widget_for_bookmark (bookmark);
	if (image_widget != NULL) {
		gtk_widget_show (image_widget);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
					       image_widget);
	}

	return menu_item;
}

gboolean
nautilus_bookmark_uri_known_not_to_exist (NautilusBookmark *bookmark)
{
	char *path_name;
	gboolean exists;

	/* Convert to a path, returning FALSE if not local. */
	if (!g_file_is_native (bookmark->details->location)) {
		return FALSE;
	}
	path_name = g_file_get_path (bookmark->details->location);

	/* Now check if the file exists (sync. call OK because it is local). */
	exists = g_file_test (path_name, G_FILE_TEST_EXISTS);
	g_free (path_name);

	return !exists;
}

void
nautilus_bookmark_set_scroll_pos (NautilusBookmark      *bookmark,
				  const char            *uri)
{
	g_free (bookmark->details->scroll_file);
	bookmark->details->scroll_file = g_strdup (uri);
}

char *
nautilus_bookmark_get_scroll_pos (NautilusBookmark      *bookmark)
{
	return g_strdup (bookmark->details->scroll_file);
}
