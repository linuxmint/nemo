/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include <string.h>

#include "nautilus-query.h"
#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>
#include <libnautilus-private/nautilus-file-utilities.h>

struct NautilusQueryDetails {
	char *text;
	char *location_uri;
	GList *mime_types;
};

static void  nautilus_query_class_init       (NautilusQueryClass *class);
static void  nautilus_query_init             (NautilusQuery      *query);

G_DEFINE_TYPE (NautilusQuery, nautilus_query, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
	NautilusQuery *query;

	query = NAUTILUS_QUERY (object);
	g_free (query->details->text);

	G_OBJECT_CLASS (nautilus_query_parent_class)->finalize (object);
}

static void
nautilus_query_class_init (NautilusQueryClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NautilusQueryDetails));
}

static void
nautilus_query_init (NautilusQuery *query)
{
	query->details = G_TYPE_INSTANCE_GET_PRIVATE (query, NAUTILUS_TYPE_QUERY,
						      NautilusQueryDetails);
}

NautilusQuery *
nautilus_query_new (void)
{
	return g_object_new (NAUTILUS_TYPE_QUERY,  NULL);
}


char *
nautilus_query_get_text (NautilusQuery *query)
{
	return g_strdup (query->details->text);
}

void 
nautilus_query_set_text (NautilusQuery *query, const char *text)
{
	g_free (query->details->text);
	query->details->text = g_strdup (text);
}

char *
nautilus_query_get_location (NautilusQuery *query)
{
	return g_strdup (query->details->location_uri);
}
	
void
nautilus_query_set_location (NautilusQuery *query, const char *uri)
{
	g_free (query->details->location_uri);
	query->details->location_uri = g_strdup (uri);
}

GList *
nautilus_query_get_mime_types (NautilusQuery *query)
{
	return eel_g_str_list_copy (query->details->mime_types);
}

void
nautilus_query_set_mime_types (NautilusQuery *query, GList *mime_types)
{
	g_list_free_full (query->details->mime_types, g_free);
	query->details->mime_types = eel_g_str_list_copy (mime_types);
}

void
nautilus_query_add_mime_type (NautilusQuery *query, const char *mime_type)
{
	query->details->mime_types = g_list_append (query->details->mime_types,
						    g_strdup (mime_type));
}

char *
nautilus_query_to_readable_string (NautilusQuery *query)
{
	if (!query || !query->details->text) {
		return g_strdup (_("Search"));
	}

	return g_strdup_printf (_("Search for \"%s\""), query->details->text);
}

static char *
encode_home_uri (const char *uri)
{
	char *home_uri;
	const char *encoded_uri;

	home_uri = nautilus_get_home_directory_uri ();

	if (g_str_has_prefix (uri, home_uri)) {
		encoded_uri = uri + strlen (home_uri);
		if (*encoded_uri == '/') {
			encoded_uri++;
		}
	} else {
		encoded_uri = uri;
	}
	
	g_free (home_uri);
	
	return g_markup_escape_text (encoded_uri, -1);
}

static char *
decode_home_uri (const char *uri)
{
	char *home_uri;
	char *decoded_uri;

	if (g_str_has_prefix (uri, "file:")) {
		decoded_uri = g_strdup (uri);
	} else {
		home_uri = nautilus_get_home_directory_uri ();

		decoded_uri = g_strconcat (home_uri, "/", uri, NULL);
		
		g_free (home_uri);
	}
		
	return decoded_uri;
}


typedef struct {
	NautilusQuery *query;
	gboolean in_text;
	gboolean in_location;
	gboolean in_mimetypes;
	gboolean in_mimetype;
	gboolean error;
} ParserInfo;

static void
start_element_cb (GMarkupParseContext *ctx,
		  const char *element_name,
		  const char **attribute_names,
		  const char **attribute_values,
		  gpointer user_data,
		  GError **err)
{
	ParserInfo *info;

	info = (ParserInfo *) user_data;

	if (strcmp (element_name, "text") == 0)
		info->in_text = TRUE;
	else if (strcmp (element_name, "location") == 0)
		info->in_location = TRUE;
	else if (strcmp (element_name, "mimetypes") == 0)
		info->in_mimetypes = TRUE;
	else if (strcmp (element_name, "mimetype") == 0)
		info->in_mimetype = TRUE;
}

static void
end_element_cb (GMarkupParseContext *ctx,
		const char *element_name,
		gpointer user_data,
		GError **err)
{
	ParserInfo *info;

	info = (ParserInfo *) user_data;

	if (strcmp (element_name, "text") == 0)
		info->in_text = FALSE;
	else if (strcmp (element_name, "location") == 0)
		info->in_location = FALSE;
	else if (strcmp (element_name, "mimetypes") == 0)
		info->in_mimetypes = FALSE;
	else if (strcmp (element_name, "mimetype") == 0)
		info->in_mimetype = FALSE;
}

static void
text_cb (GMarkupParseContext *ctx,
	 const char *text,
	 gsize text_len,
	 gpointer user_data,
	 GError **err)
{
	ParserInfo *info;
	char *t, *uri;

	info = (ParserInfo *) user_data;

	t = g_strndup (text, text_len);
	
	if (info->in_text) {
		nautilus_query_set_text (info->query, t);
	} else if (info->in_location) {
		uri = decode_home_uri (t);
		nautilus_query_set_location (info->query, uri);
		g_free (uri);
	} else if (info->in_mimetypes && info->in_mimetype) {
		nautilus_query_add_mime_type (info->query, t);
	}
	
	g_free (t);

}

static void
error_cb (GMarkupParseContext *ctx,
	  GError *err,
	  gpointer user_data)
{
	ParserInfo *info;

	info = (ParserInfo *) user_data;

	info->error = TRUE;
}

static GMarkupParser parser = {
	start_element_cb,
	end_element_cb,
	text_cb,
	NULL,
	error_cb
};


static NautilusQuery *
nautilus_query_parse_xml (char *xml, gsize xml_len)
{
	ParserInfo info = { NULL };
	GMarkupParseContext *ctx;

	if (xml_len == -1) {
		xml_len = strlen (xml);
	}
	
	info.query = nautilus_query_new ();
	info.in_text = FALSE;
	info.error = FALSE;

	ctx = g_markup_parse_context_new (&parser, 0, &info, NULL);
	g_markup_parse_context_parse (ctx, xml, xml_len, NULL);

	if (info.error) {
		g_object_unref (info.query);
		return NULL;
	}

	return info.query;
}


NautilusQuery *
nautilus_query_load (char *file)
{
	NautilusQuery *query;
	char *xml;
	gsize xml_len;
	
	if (!g_file_test (file, G_FILE_TEST_EXISTS)) {
		return NULL;
	}
	

	g_file_get_contents (file, &xml, &xml_len, NULL);

	if (xml_len == 0) {
		return NULL;
	}

	query = nautilus_query_parse_xml (xml, xml_len);
	g_free (xml);

	return query;
}

static char *
nautilus_query_to_xml (NautilusQuery *query)
{
	GString *xml;
	char *text;
	char *uri;
	char *mimetype;
	GList *l;

	xml = g_string_new ("");
	g_string_append (xml,
			 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			 "<query version=\"1.0\">\n");

	text = g_markup_escape_text (query->details->text, -1);
	g_string_append_printf (xml, "   <text>%s</text>\n", text);
	g_free (text);

	if (query->details->location_uri) {
		uri = encode_home_uri (query->details->location_uri);
		g_string_append_printf (xml, "   <location>%s</location>\n", uri);
		g_free (uri);
	}

	if (query->details->mime_types) {
		g_string_append (xml, "   <mimetypes>\n");
		for (l = query->details->mime_types; l != NULL; l = l->next) {
			mimetype = g_markup_escape_text (l->data, -1);
			g_string_append_printf (xml, "      <mimetype>%s</mimetype>\n", mimetype);
			g_free (mimetype);
		}
		g_string_append (xml, "   </mimetypes>\n");
	}
	
	g_string_append (xml, "</query>\n");

	return g_string_free (xml, FALSE);
}

gboolean
nautilus_query_save (NautilusQuery *query, char *file)
{
	char *xml;
	GError *err = NULL;
	gboolean res;


	res = TRUE;
	xml = nautilus_query_to_xml (query);
	g_file_set_contents (file, xml, strlen (xml), &err);
	g_free (xml);
	
	if (err != NULL) {
		res = FALSE;
		g_error_free (err);
	}
	return res;
}
