/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc
 * Copyright (c) 2007 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 * XMP support by Hubert Figuiere <hfiguiere@novell.com>
 */

#include <config.h>
#include "nemo-image-properties-page.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-vfs-extensions.h>
#include <libnemo-extension/nemo-property-page-provider.h>
#include <libnemo-private/nemo-module.h>
#include <string.h>

#ifdef HAVE_EXIF
  #include <libexif/exif-data.h>
  #include <libexif/exif-ifd.h>
  #include <libexif/exif-loader.h>
#endif
#ifdef HAVE_EXEMPI
  #include <exempi/xmp.h>
  #include <exempi/xmpconsts.h>
#endif

#define LOAD_BUFFER_SIZE 8192

struct NemoImagePropertiesPageDetails {
	GCancellable *cancellable;
	GtkWidget *grid;
	GdkPixbufLoader *loader;
	gboolean got_size;
	gboolean pixbuf_still_loading;
	char buffer[LOAD_BUFFER_SIZE];
	int width;
	int height;
#ifdef HAVE_EXIF
	ExifLoader *exifldr;
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
	XmpPtr     xmp;
#endif
};

#ifdef HAVE_EXIF
struct ExifAttribute {
	ExifTag tag;
	char *value;
	gboolean found;
};
#endif /*HAVE_EXIF*/

enum {
	PROP_URI
};

typedef struct {
        GObject parent;
} NemoImagePropertiesPageProvider;

typedef struct {
        GObjectClass parent;
} NemoImagePropertiesPageProviderClass;


static GType nemo_image_properties_page_provider_get_type (void);
static void  property_page_provider_iface_init                (NemoPropertyPageProviderIface *iface);


G_DEFINE_TYPE (NemoImagePropertiesPage, nemo_image_properties_page, GTK_TYPE_BOX);

G_DEFINE_TYPE_WITH_CODE (NemoImagePropertiesPageProvider, nemo_image_properties_page_provider, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NEMO_TYPE_PROPERTY_PAGE_PROVIDER,
						property_page_provider_iface_init));

static void
nemo_image_properties_page_finalize (GObject *object)
{
	NemoImagePropertiesPage *page;

	page = NEMO_IMAGE_PROPERTIES_PAGE (object);

	if (page->details->cancellable) {
		g_cancellable_cancel (page->details->cancellable);
		g_object_unref (page->details->cancellable);
		page->details->cancellable = NULL;
	}

	G_OBJECT_CLASS (nemo_image_properties_page_parent_class)->finalize (object);
}

static void
file_close_callback (GObject      *object,
		     GAsyncResult *res,
		     gpointer      data)
{
	NemoImagePropertiesPage *page;
	GInputStream *stream;

	page = NEMO_IMAGE_PROPERTIES_PAGE (data);
	stream = G_INPUT_STREAM (object);

	g_input_stream_close_finish (stream, res, NULL);

	g_object_unref (page->details->cancellable);
	page->details->cancellable = NULL;
}

static void
append_item (NemoImagePropertiesPage *page,
	     const char                  *name,
	     const char                  *value)
{
	GtkWidget *name_label;
	GtkWidget *label;
	PangoAttrList *attrs;

	name_label = gtk_label_new (name);
	attrs = pango_attr_list_new ();
	pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));
	gtk_label_set_attributes (GTK_LABEL (name_label), attrs);
	pango_attr_list_unref (attrs);
	gtk_misc_set_alignment (GTK_MISC (name_label), 0, 0);
	gtk_container_add (GTK_CONTAINER (page->details->grid), name_label);
	gtk_widget_show (name_label);

	if (value != NULL) {
		label = gtk_label_new (value);
		gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
		gtk_misc_set_alignment (GTK_MISC (label), 0, 0);
		gtk_grid_attach_next_to (GTK_GRID (page->details->grid), label,
					 name_label, GTK_POS_RIGHT,
					 1, 1);
		gtk_widget_show (label);
	}
}

#ifdef HAVE_EXIF
static char *
exif_string_to_utf8 (const char *exif_str)
{
	char *utf8_str;
	
	if (g_utf8_validate (exif_str, -1, NULL)) {
		return g_strdup (exif_str);
	}
	
	utf8_str = g_locale_to_utf8 (exif_str, -1, NULL, NULL, NULL);
	if (utf8_str != NULL) {
		return utf8_str;
	}
	
	return eel_make_valid_utf8 (exif_str);
}

static void
exif_content_callback (ExifContent *content, gpointer data)
{
	struct ExifAttribute *attribute;
	char b[1024];

	attribute = (struct ExifAttribute *)data;
	if (attribute->found) {
		return;
	}

        attribute->value = g_strdup (exif_content_get_value (content, attribute->tag, b, sizeof(b)));

	if (attribute->value != NULL) {
		attribute->found = TRUE;
	}
}

static char *
exifdata_get_tag_name_utf8 (ExifTag tag) 
{
	return exif_string_to_utf8 (exif_tag_get_name (tag));
}

static char *
exifdata_get_tag_value_utf8 (ExifData *data, ExifTag tag) 
{
	struct ExifAttribute attribute;
	char *utf8_value;

	attribute.tag = tag;
	attribute.value = NULL;
	attribute.found = FALSE;
	
	exif_data_foreach_content (data, exif_content_callback, &attribute);

	if (attribute.found) {
		utf8_value = exif_string_to_utf8 (attribute.value);
		g_free (attribute.value);
	} else {
		utf8_value = NULL;
	}

	return utf8_value;
}

static gboolean
append_tag_value_pair (NemoImagePropertiesPage *page,
		       ExifData *data,
		       ExifTag   tag,
		       char     *description) 
{
        char *utf_attribute;
        char *utf_value;

	utf_attribute = exifdata_get_tag_name_utf8 (tag);
	utf_value = exifdata_get_tag_value_utf8 (data, tag);

	if ((utf_attribute == NULL) || (utf_value == NULL)) {
		g_free (utf_attribute);
		g_free (utf_value);
   		return FALSE;
	}

	append_item (page,
		     description ? description : utf_attribute,
		     utf_value);

        g_free (utf_attribute);
        g_free (utf_value);

	return TRUE;
}
#endif /*HAVE_EXIF*/


#ifdef HAVE_EXEMPI
static void
append_xmp_value_pair (NemoImagePropertiesPage *page,
		       XmpPtr      xmp,
		       const char *ns,
		       const char *propname,
		       char       *descr)
{
	uint32_t options;
	XmpStringPtr value;

	value = xmp_string_new();
	if (xmp_get_property (xmp, ns, propname, value, &options)) {
		if (XMP_IS_PROP_SIMPLE (options)) {
			append_item (page, descr, xmp_string_cstr (value));
		}
		else if (XMP_IS_PROP_ARRAY (options)) {
			XmpIteratorPtr iter;

			iter = xmp_iterator_new (xmp, ns, propname, XMP_ITER_JUSTLEAFNODES);
			if (iter) {
				GString *str;
				gboolean first = TRUE;

				str = g_string_new (NULL);

				while (xmp_iterator_next (iter, NULL, NULL, value, &options) 
				       && !XMP_IS_PROP_QUALIFIER(options)) {
					if (!first) {
						g_string_append_printf (str, ", ");
					}
					else {
						first = FALSE;
					}
					g_string_append_printf (str,
								"%s",
								xmp_string_cstr(value));
				}
				xmp_iterator_free(iter);
				append_item (page, descr, g_string_free (str, FALSE));
			}
		}
	}
	xmp_string_free(value);
}
#endif /*HAVE EXEMPI*/

static gboolean
append_option_value_pair (NemoImagePropertiesPage *page,
			  GdkPixbuf                   *pixbuf,
			  const char                  *key,
			  char                        *description)
{
	const char *value;

	value = gdk_pixbuf_get_option (pixbuf, key);
	if (value == NULL)
		return FALSE;

	append_item (page, description, value);
	return TRUE;
}

static void
append_basic_info (NemoImagePropertiesPage *page)
{
	GdkPixbufFormat *format;
	char *name;
	char *desc;
	char *value;

	format = gdk_pixbuf_loader_get_format (page->details->loader);

	name = gdk_pixbuf_format_get_name (format);
	desc = gdk_pixbuf_format_get_description (format);
	value = g_strdup_printf ("%s (%s)", name, desc);
	g_free (name);
	g_free (desc);
	append_item (page, _("Image Type"), value);
	g_free (value);
	value = g_strdup_printf (ngettext ("%d pixel",
					   "%d pixels",
					   page->details->width),
				 page->details->width);
	append_item (page, _("Width"), value);
	g_free (value);
	value = g_strdup_printf (ngettext ("%d pixel",
					   "%d pixels",
					   page->details->height),
				 page->details->height);
	append_item (page, _("Height"), value);
	g_free (value);
}

static void
append_options_info (NemoImagePropertiesPage *page)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_loader_get_pixbuf (page->details->loader);
	if (pixbuf == NULL)
		return;

	if (!append_option_value_pair (page, pixbuf, "Title", _("Title")))
		append_option_value_pair (page, pixbuf, "tEXt::Title", _("Title"));
	if (!append_option_value_pair (page, pixbuf, "Author", _("Author")))
		append_option_value_pair (page, pixbuf, "tEXt::Author", _("Author"));

	append_option_value_pair (page, pixbuf, "tEXt::Description", _("Description"));
	append_option_value_pair (page, pixbuf, "tEXt::Copyright", _("Copyright"));
	append_option_value_pair (page, pixbuf, "tEXt::Creation Time", _("Created On"));
	append_option_value_pair (page, pixbuf, "tEXt::Software", _("Created By"));
	append_option_value_pair (page, pixbuf, "tEXt::Disclaimer", _("Disclaimer"));
	append_option_value_pair (page, pixbuf, "tEXt::Warning", _("Warning"));
	append_option_value_pair (page, pixbuf, "tEXt::Source", _("Source"));
	append_option_value_pair (page, pixbuf, "tEXt::Comment", _("Comment"));
}

static void
append_exif_info (NemoImagePropertiesPage *page)
{
#ifdef HAVE_EXIF
	ExifData *exifdata;

	exifdata = exif_loader_get_data (page->details->exifldr);
	if (exifdata == NULL)
		return;

	if (exifdata->ifd[0] && exifdata->ifd[0]->count) {
                append_tag_value_pair (page, exifdata, EXIF_TAG_MAKE, _("Camera Brand"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_MODEL, _("Camera Model"));

                /* Choose which date to show in order of relevance */
                if (!append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME_ORIGINAL, _("Date Taken"))) {
			if (!append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME_DIGITIZED, _("Date Digitized"))) {
				append_tag_value_pair (page, exifdata, EXIF_TAG_DATE_TIME, _("Date Modified"));
			}
		}

                append_tag_value_pair (page, exifdata, EXIF_TAG_EXPOSURE_TIME, _("Exposure Time"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_APERTURE_VALUE, _("Aperture Value"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_ISO_SPEED_RATINGS, _("ISO Speed Rating"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_FLASH,_("Flash Fired"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_METERING_MODE, _("Metering Mode"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_EXPOSURE_PROGRAM, _("Exposure Program"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_FOCAL_LENGTH,_("Focal Length"));
                append_tag_value_pair (page, exifdata, EXIF_TAG_SOFTWARE, _("Software"));
	}

	exif_data_unref (exifdata);
#endif
}

static void
append_xmp_info (NemoImagePropertiesPage *page)
{
#ifdef HAVE_EXEMPI
	if (page->details->xmp == NULL)
		return;

	append_xmp_value_pair (page, page->details->xmp, NS_IPTC4XMP, "Location", _("Location"));
	append_xmp_value_pair (page, page->details->xmp, NS_DC, "description", _("Description"));
	append_xmp_value_pair (page, page->details->xmp, NS_DC, "subject", _("Keywords"));
	append_xmp_value_pair (page, page->details->xmp, NS_DC, "creator", _("Creator"));
	append_xmp_value_pair (page, page->details->xmp, NS_DC, "rights", _("Copyright"));
	append_xmp_value_pair (page, page->details->xmp, NS_XAP,"Rating", _("Rating"));
	/* TODO add CC licenses */
#endif /*HAVE EXEMPI*/
}

static void
load_finished (NemoImagePropertiesPage *page)
{
	GtkWidget *label;

	label = gtk_grid_get_child_at (GTK_GRID (page->details->grid), 0, 0);
	gtk_container_remove (GTK_CONTAINER (page->details->grid), label);

	if (page->details->loader != NULL) {
		gdk_pixbuf_loader_close (page->details->loader, NULL);
	}

	if (page->details->got_size) {
		append_basic_info (page);
		append_options_info (page);
		append_exif_info (page);
		append_xmp_info (page);
	} else {
		append_item (page, _("Failed to load image information"), NULL);
	}

	if (page->details->loader != NULL) {
		g_object_unref (page->details->loader);
		page->details->loader = NULL;
	}
#ifdef HAVE_EXIF
	if (page->details->exifldr != NULL) {
		exif_loader_unref (page->details->exifldr);
		page->details->exifldr = NULL;
	}
#endif /*HAVE_EXIF*/
#ifdef HAVE_EXEMPI
	if (page->details->xmp != NULL) {
		xmp_free (page->details->xmp);
		page->details->xmp = NULL;
	}
#endif
}

static void
file_read_callback (GObject      *object,
		    GAsyncResult *res,
		    gpointer      data)
{
	NemoImagePropertiesPage *page;
	GInputStream *stream;
	gssize count_read;
	GError *error;
	int exif_still_loading;
	gboolean done_reading;

	page = NEMO_IMAGE_PROPERTIES_PAGE (data);
	stream = G_INPUT_STREAM (object);

	error = NULL;
	done_reading = FALSE;
	count_read = g_input_stream_read_finish (stream, res, &error);

	if (count_read > 0) {

		g_assert (count_read <= sizeof(page->details->buffer));

#ifdef HAVE_EXIF
		exif_still_loading = exif_loader_write (page->details->exifldr,
				  		        (guchar *) page->details->buffer,
				  			count_read);
#else
		exif_still_loading = 0;
#endif

		if (page->details->pixbuf_still_loading) {
			if (!gdk_pixbuf_loader_write (page->details->loader,
					      	      (const guchar *) page->details->buffer,
					      	      count_read,
					      	      NULL)) {
				page->details->pixbuf_still_loading = FALSE;
			}
		}

		if (page->details->pixbuf_still_loading ||
		    (exif_still_loading == 1)) {
			g_input_stream_read_async (G_INPUT_STREAM (stream),
						   page->details->buffer,
						   sizeof (page->details->buffer),
						   0,
						   page->details->cancellable,
						   file_read_callback,
						   page);
		}
		else {
			done_reading = TRUE;
		}
	}
	else {
		/* either EOF, cancelled or an error occurred */
		done_reading = TRUE;
	}

	if (error != NULL) {
		char *uri = g_file_get_uri (G_FILE (object));
		g_warning ("Error reading %s: %s", uri, error->message);
		g_free (uri);
		g_clear_error (&error);
	}

	if (done_reading) {
		load_finished (page);
		g_input_stream_close_async (stream,
					    0,
					    page->details->cancellable,
					    file_close_callback,
					    page);
	}
}

static void
size_prepared_callback (GdkPixbufLoader *loader, 
			int              width,
			int              height,
			gpointer         callback_data)
{
	NemoImagePropertiesPage *page;

	page = NEMO_IMAGE_PROPERTIES_PAGE (callback_data);

	page->details->height = height;
	page->details->width = width;
	page->details->got_size = TRUE;
	page->details->pixbuf_still_loading = FALSE;
}

typedef struct {
	NemoImagePropertiesPage *page;
	NemoFileInfo            *info;
} FileOpenData;

static void
file_open_callback (GObject      *object,
		    GAsyncResult *res,
		    gpointer      user_data)
{
	FileOpenData *data = user_data;
	NemoImagePropertiesPage *page = data->page;
	GFile *file;
	GFileInputStream *stream;
	GError *error;
	char *uri;

	file = G_FILE (object);
	uri = g_file_get_uri (file);
	
	error = NULL;
	stream = g_file_read_finish (file, res, &error);
	if (stream) {
		char *mime_type;

		mime_type = nemo_file_info_get_mime_type (data->info);
		page->details->loader = gdk_pixbuf_loader_new_with_mime_type (mime_type, &error);
		if (error != NULL) {
			g_warning ("Error creating loader for %s: %s", uri, error->message);
			g_clear_error (&error);
		}
		page->details->pixbuf_still_loading = TRUE;
		page->details->width = 0;
		page->details->height = 0;
#ifdef HAVE_EXIF
		page->details->exifldr = exif_loader_new ();
#endif /*HAVE_EXIF*/
		g_free (mime_type);

		g_signal_connect (page->details->loader,
				  "size_prepared",
				  G_CALLBACK (size_prepared_callback),
				  page);

		g_input_stream_read_async (G_INPUT_STREAM (stream),
					   page->details->buffer,
					   sizeof (page->details->buffer),
					   0,
					   page->details->cancellable,
					   file_read_callback,
					   page);

		g_object_unref (stream);
	} else {
		g_warning ("Error reading %s: %s", uri, error->message);
		g_clear_error (&error);
		load_finished (page);
	}

	g_free (uri);
	g_free (data);
}

static void
load_location (NemoImagePropertiesPage *page,
	       NemoFileInfo	       *info)
{
	GFile *file;
	char *uri;
	FileOpenData *data;

	g_assert (NEMO_IS_IMAGE_PROPERTIES_PAGE (page));
	g_assert (info != NULL);

	page->details->cancellable = g_cancellable_new ();

	uri = nemo_file_info_get_uri (info);
	file = g_file_new_for_uri (uri);

#ifdef HAVE_EXEMPI
	{
		/* Current Exempi does not support setting custom IO to be able to use Gnome-vfs */
		/* So it will only work with local files. Future version might remove this limitation */
		XmpFilePtr xf;
		char *localname;

		localname = g_filename_from_uri (uri, NULL, NULL);
		if (localname) {
			xf = xmp_files_open_new (localname, 0);
			page->details->xmp = xmp_files_get_new_xmp (xf); /* only load when loading */
			xmp_files_close (xf, 0);
			g_free (localname);
		}
		else {
			page->details->xmp = NULL;
		}
	}
#endif /*HAVE_EXEMPI*/

	data = g_new0 (FileOpenData, 1);
	data->page = page;
	data->info = info;
	
	g_file_read_async (file,
			   0,
			   page->details->cancellable,
			   file_open_callback,
			   data);

	g_object_unref (file);
	g_free (uri);
}

static void
nemo_image_properties_page_class_init (NemoImagePropertiesPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = nemo_image_properties_page_finalize;

	g_type_class_add_private (object_class, sizeof(NemoImagePropertiesPageDetails));
}

static void
nemo_image_properties_page_init (NemoImagePropertiesPage *page)
{
	GtkWidget *sw;

	page->details = G_TYPE_INSTANCE_GET_PRIVATE (page,
						     NEMO_TYPE_IMAGE_PROPERTIES_PAGE,
						     NemoImagePropertiesPageDetails);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (page), GTK_ORIENTATION_VERTICAL);
	gtk_box_set_homogeneous (GTK_BOX (page), FALSE);
	gtk_box_set_spacing (GTK_BOX (page), 0);
	gtk_container_set_border_width (GTK_CONTAINER (page), 0);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (sw), 0);
	gtk_widget_set_vexpand (GTK_WIDGET (sw), TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start (GTK_BOX (page), sw, FALSE, TRUE, 2);

	page->details->grid = gtk_grid_new ();
	gtk_container_set_border_width (GTK_CONTAINER (page->details->grid), 6);
	gtk_orientable_set_orientation (GTK_ORIENTABLE (page->details->grid), GTK_ORIENTATION_VERTICAL);
	gtk_grid_set_row_spacing (GTK_GRID (page->details->grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (page->details->grid), 20);
	append_item (page, _("Loading..."), NULL);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), page->details->grid);

	gtk_widget_show_all (GTK_WIDGET (page));
}

static gboolean
is_mime_type_supported (const char *mime_type)
{
	gboolean supported;
	GSList *formats;
	GSList *l;

	supported = FALSE;
	formats = gdk_pixbuf_get_formats ();

	for (l = formats; supported == FALSE && l != NULL; l = l->next) {
		GdkPixbufFormat *format = l->data;
		char **mime_types = gdk_pixbuf_format_get_mime_types (format);
		int i;

		for (i = 0; mime_types[i] != NULL; i++) {
			if (strcmp (mime_types[i], mime_type) == 0) {
				supported = TRUE;
				break;
			}
		}
		g_strfreev (mime_types);
	}
	g_slist_free (formats);

	return supported;
}

static GList *
get_property_pages (NemoPropertyPageProvider *provider,
                    GList *files)
{
	GList *pages;
	NemoFileInfo *file;

	char *mime_type;
	
	/* Only show the property page if 1 file is selected */
	if (!files || files->next != NULL) {
		return NULL;
	}

	pages = NULL;
	
        file = NEMO_FILE_INFO (files->data);

	mime_type = nemo_file_info_get_mime_type (file);
	if (mime_type != NULL && is_mime_type_supported (mime_type)) {
		NemoImagePropertiesPage *page;
		NemoPropertyPage *real_page;

		page = g_object_new (nemo_image_properties_page_get_type (), NULL);
		load_location (page, file);

		real_page = nemo_property_page_new ("NemoImagePropertiesPage::property_page",
		                            	    gtk_label_new (_("Image")),
		                                    GTK_WIDGET (page));
		pages = g_list_append (pages, real_page);
	}

        g_free (mime_type);

	return pages;
}

static void 
property_page_provider_iface_init (NemoPropertyPageProviderIface *iface)
{
	iface->get_pages = get_property_pages;
}


static void
nemo_image_properties_page_provider_init (NemoImagePropertiesPageProvider *sidebar)
{
}

static void
nemo_image_properties_page_provider_class_init (NemoImagePropertiesPageProviderClass *class)
{
}

void
nemo_image_properties_page_register (void)
{
        nemo_module_add_type (nemo_image_properties_page_provider_get_type ());
}

