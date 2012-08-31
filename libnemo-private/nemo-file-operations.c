/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-file-operations.c - Nemo file operations.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2007 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Authors: Alexander Larsson <alexl@redhat.com>
            Ettore Perazzoli <ettore@gnu.org> 
            Pavel Cisler <pavel@eazel.com> 
 */

#include <config.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <locale.h>
#include <math.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

#include "nemo-file-operations.h"

#include "nemo-file-changes-queue.h"
#include "nemo-lib-self-check-functions.h"

#include "nemo-progress-info.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include "nemo-file-changes-queue.h"
#include "nemo-file-private.h"
#include "nemo-desktop-icon-file.h"
#include "nemo-desktop-link-monitor.h"
#include "nemo-global-preferences.h"
#include "nemo-link.h"
#include "nemo-trash-monitor.h"
#include "nemo-file-utilities.h"
#include "nemo-file-conflict-dialog.h"
#include "nemo-file-undo-operations.h"
#include "nemo-file-undo-manager.h"

/* TODO: TESTING!!! */

typedef struct {
	GIOSchedulerJob *io_job;	
	GTimer *time;
	GtkWindow *parent_window;
	int screen_num;
	int inhibit_cookie;
	NemoProgressInfo *progress;
	GCancellable *cancellable;
	GHashTable *skip_files;
	GHashTable *skip_readdir_error;
	NemoFileUndoInfo *undo_info;
	gboolean skip_all_error;
	gboolean skip_all_conflict;
	gboolean merge_all;
	gboolean replace_all;
	gboolean delete_all;
} CommonJob;

typedef struct {
	CommonJob common;
	gboolean is_move;
	GList *files;
	GFile *destination;
	GFile *desktop_location;
	GFile *fake_display_source;
	GdkPoint *icon_positions;
	int n_icon_positions;
	GHashTable *debuting_files;
	gchar *target_name;
	NemoCopyCallback  done_callback;
	gpointer done_callback_data;
} CopyMoveJob;

typedef struct {
	CommonJob common;
	GList *files;
	gboolean try_trash;
	gboolean user_cancel;
	NemoDeleteCallback done_callback;
	gpointer done_callback_data;
} DeleteJob;

typedef struct {
	CommonJob common;
	GFile *dest_dir;
	char *filename;
	gboolean make_dir;
	GFile *src;
	char *src_data;
	int length;
	GdkPoint position;
	gboolean has_position;
	GFile *created_file;
	NemoCreateCallback done_callback;
	gpointer done_callback_data;
} CreateJob;


typedef struct {
	CommonJob common;
	GList *trash_dirs;
	gboolean should_confirm;
	NemoOpCallback done_callback;
	gpointer done_callback_data;
} EmptyTrashJob;

typedef struct {
	CommonJob common;
	GFile *file;
	gboolean interactive;
	NemoOpCallback done_callback;
	gpointer done_callback_data;
} MarkTrustedJob;

typedef struct {
	CommonJob common;
	GFile *file;
	NemoOpCallback done_callback;
	gpointer done_callback_data;
	guint32 file_permissions;
	guint32 file_mask;
	guint32 dir_permissions;
	guint32 dir_mask;
} SetPermissionsJob;

typedef enum {
	OP_KIND_COPY,
	OP_KIND_MOVE,
	OP_KIND_DELETE,
	OP_KIND_TRASH
} OpKind;

typedef struct {
	int num_files;
	goffset num_bytes;
	int num_files_since_progress;
	OpKind op;
} SourceInfo;

typedef struct {
	int num_files;
	goffset num_bytes;
	OpKind op;
	guint64 last_report_time;
	int last_reported_files_left;
} TransferInfo;

#define SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE 15
#define NSEC_PER_MICROSEC 1000

#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH 50

#define IS_IO_ERROR(__error, KIND) (((__error)->domain == G_IO_ERROR && (__error)->code == G_IO_ERROR_ ## KIND))

#define SKIP _("_Skip")
#define SKIP_ALL _("S_kip All")
#define RETRY _("_Retry")
#define DELETE_ALL _("Delete _All")
#define REPLACE _("_Replace")
#define REPLACE_ALL _("Replace _All")
#define MERGE _("_Merge")
#define MERGE_ALL _("Merge _All")
#define COPY_FORCE _("Copy _Anyway")

static void
mark_desktop_file_trusted (CommonJob *common,
			   GCancellable *cancellable,
			   GFile *file,
			   gboolean interactive);

static gboolean
is_all_button_text (const char *button_text)
{
	g_assert (button_text != NULL);

	return !strcmp (button_text, SKIP_ALL) ||
	       !strcmp (button_text, REPLACE_ALL) ||
	       !strcmp (button_text, DELETE_ALL) ||
	       !strcmp (button_text, MERGE_ALL);
}

static void scan_sources (GList *files,
			  SourceInfo *source_info,
			  CommonJob *job,
			  OpKind kind);


static gboolean empty_trash_job (GIOSchedulerJob *io_job,
				 GCancellable *cancellable,
				 gpointer user_data);

static char * query_fs_type (GFile *file,
			     GCancellable *cancellable);

/* keep in time with format_time()
 *
 * This counts and outputs the number of “time units”
 * formatted and displayed by format_time().
 * For instance, if format_time outputs “3 hours, 4 minutes”
 * it yields 7.
 */
static int
seconds_count_format_time_units (int seconds)
{
	int minutes;
	int hours;

	if (seconds < 0) {
		/* Just to make sure... */
		seconds = 0;
	}

	if (seconds < 60) {
		/* seconds */
		return seconds;
	}

	if (seconds < 60*60) {
		/* minutes */
		minutes = seconds / 60;
		return minutes;
	}

	hours = seconds / (60*60);

	if (seconds < 60*60*4) {
		/* minutes + hours */
		minutes = (seconds - hours * 60 * 60) / 60;
		return minutes + hours;
	}

	return hours;
}

static char *
format_time (int seconds)
{
	int minutes;
	int hours;
	char *res;

	if (seconds < 0) {
		/* Just to make sure... */
		seconds = 0;
	}
	
	if (seconds < 60) {
		return g_strdup_printf (ngettext ("%'d second","%'d seconds", (int) seconds), (int) seconds);
	}

	if (seconds < 60*60) {
		minutes = seconds / 60;
		return g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
	}

	hours = seconds / (60*60);
	
	if (seconds < 60*60*4) {
		char *h, *m;

		minutes = (seconds - hours * 60 * 60) / 60;
		
		h = g_strdup_printf (ngettext ("%'d hour", "%'d hours", hours), hours);
		m = g_strdup_printf (ngettext ("%'d minute", "%'d minutes", minutes), minutes);
		res = g_strconcat (h, ", ", m, NULL);
		g_free (h);
		g_free (m);
		return res;
	}
	
	return g_strdup_printf (ngettext ("approximately %'d hour",
					  "approximately %'d hours",
					  hours), hours);
}

static char *
shorten_utf8_string (const char *base, int reduce_by_num_bytes)
{
	int len;
	char *ret;
	const char *p;
	
	len = strlen (base);
	len -= reduce_by_num_bytes;
	
	if (len <= 0) {
		return NULL;
	}

	ret = g_new (char, len + 1);

	p = base;
	while (len) {
		char *next;
		next = g_utf8_next_char (p);
		if (next - p > len || *next == '\0') {
			break;
		}
		
		len -= next - p;
		p = next;
	}
	
	if (p - base == 0) {
		g_free (ret);
		return NULL;
	} else {
		memcpy (ret, base, p - base);
		ret[p - base] = '\0';
		return ret;
	}
}

/* Note that we have these two separate functions with separate format
 * strings for ease of localization.
 */

static char *
get_link_name (const char *name, int count, int max_length)
{
	const char *format;
	char *result;
	int unshortened_length;
	gboolean use_count;
	
	g_assert (name != NULL);

	if (count < 0) {
		g_warning ("bad count in get_link_name");
		count = 0;
	}

	if (count <= 2) {
		/* Handle special cases for low numbers.
		 * Perhaps for some locales we will need to add more.
		 */
		switch (count) {
		default:
			g_assert_not_reached ();
			/* fall through */
		case 0:
			/* duplicate original file name */
			format = "%s";
			break;
		case 1:
			/* appended to new link file */
			format = _("Link to %s");
			break;
		case 2:
			/* appended to new link file */
			format = _("Another link to %s");
			break;
		}

		use_count = FALSE;
	} else {
		/* Handle special cases for the first few numbers of each ten.
		 * For locales where getting this exactly right is difficult,
		 * these can just be made all the same as the general case below.
		 */
		switch (count % 10) {
		case 1:
			/* Localizers: Feel free to leave out the "st" suffix
			 * if there's no way to do that nicely for a
			 * particular language.
			 */
			format = _("%'dst link to %s");
			break;
		case 2:
			/* appended to new link file */
			format = _("%'dnd link to %s");
			break;
		case 3:
			/* appended to new link file */
			format = _("%'drd link to %s");
			break;
		default:
			/* appended to new link file */
			format = _("%'dth link to %s");
			break;
		}

		use_count = TRUE;
	}

	if (use_count)
		result = g_strdup_printf (format, count, name);
	else
		result = g_strdup_printf (format, name);

	if (max_length > 0 && (unshortened_length = strlen (result)) > max_length) {
		char *new_name;

		new_name = shorten_utf8_string (name, unshortened_length - max_length);
		if (new_name) {
			g_free (result);

			if (use_count)
				result = g_strdup_printf (format, count, new_name);
			else
				result = g_strdup_printf (format, new_name);

			g_assert (strlen (result) <= max_length);
			g_free (new_name);
		}
	}

	return result;
}


/* Localizers: 
 * Feel free to leave out the st, nd, rd and th suffix or
 * make some or all of them match.
 */

/* localizers: tag used to detect the first copy of a file */
static const char untranslated_copy_duplicate_tag[] = N_(" (copy)");
/* localizers: tag used to detect the second copy of a file */
static const char untranslated_another_copy_duplicate_tag[] = N_(" (another copy)");

/* localizers: tag used to detect the x11th copy of a file */
static const char untranslated_x11th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x12th copy of a file */
static const char untranslated_x12th_copy_duplicate_tag[] = N_("th copy)");
/* localizers: tag used to detect the x13th copy of a file */
static const char untranslated_x13th_copy_duplicate_tag[] = N_("th copy)");

/* localizers: tag used to detect the x1st copy of a file */
static const char untranslated_st_copy_duplicate_tag[] = N_("st copy)");
/* localizers: tag used to detect the x2nd copy of a file */
static const char untranslated_nd_copy_duplicate_tag[] = N_("nd copy)");
/* localizers: tag used to detect the x3rd copy of a file */
static const char untranslated_rd_copy_duplicate_tag[] = N_("rd copy)");

/* localizers: tag used to detect the xxth copy of a file */
static const char untranslated_th_copy_duplicate_tag[] = N_("th copy)");

#define COPY_DUPLICATE_TAG _(untranslated_copy_duplicate_tag)
#define ANOTHER_COPY_DUPLICATE_TAG _(untranslated_another_copy_duplicate_tag)
#define X11TH_COPY_DUPLICATE_TAG _(untranslated_x11th_copy_duplicate_tag)
#define X12TH_COPY_DUPLICATE_TAG _(untranslated_x12th_copy_duplicate_tag)
#define X13TH_COPY_DUPLICATE_TAG _(untranslated_x13th_copy_duplicate_tag)

#define ST_COPY_DUPLICATE_TAG _(untranslated_st_copy_duplicate_tag)
#define ND_COPY_DUPLICATE_TAG _(untranslated_nd_copy_duplicate_tag)
#define RD_COPY_DUPLICATE_TAG _(untranslated_rd_copy_duplicate_tag)
#define TH_COPY_DUPLICATE_TAG _(untranslated_th_copy_duplicate_tag)

/* localizers: appended to first file copy */
static const char untranslated_first_copy_duplicate_format[] = N_("%s (copy)%s");
/* localizers: appended to second file copy */
static const char untranslated_second_copy_duplicate_format[] = N_("%s (another copy)%s");

/* localizers: appended to x11th file copy */
static const char untranslated_x11th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");
/* localizers: appended to x12th file copy */
static const char untranslated_x12th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");
/* localizers: appended to x13th file copy */
static const char untranslated_x13th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");

/* localizers: if in your language there's no difference between 1st, 2nd, 3rd and nth
 * plurals, you can leave the st, nd, rd suffixes out and just make all the translated
 * strings look like "%s (copy %'d)%s".
 */

/* localizers: appended to x1st file copy */
static const char untranslated_st_copy_duplicate_format[] = N_("%s (%'dst copy)%s");
/* localizers: appended to x2nd file copy */
static const char untranslated_nd_copy_duplicate_format[] = N_("%s (%'dnd copy)%s");
/* localizers: appended to x3rd file copy */
static const char untranslated_rd_copy_duplicate_format[] = N_("%s (%'drd copy)%s");
/* localizers: appended to xxth file copy */
static const char untranslated_th_copy_duplicate_format[] = N_("%s (%'dth copy)%s");

#define FIRST_COPY_DUPLICATE_FORMAT _(untranslated_first_copy_duplicate_format)
#define SECOND_COPY_DUPLICATE_FORMAT _(untranslated_second_copy_duplicate_format)
#define X11TH_COPY_DUPLICATE_FORMAT _(untranslated_x11th_copy_duplicate_format)
#define X12TH_COPY_DUPLICATE_FORMAT _(untranslated_x12th_copy_duplicate_format)
#define X13TH_COPY_DUPLICATE_FORMAT _(untranslated_x13th_copy_duplicate_format)

#define ST_COPY_DUPLICATE_FORMAT _(untranslated_st_copy_duplicate_format)
#define ND_COPY_DUPLICATE_FORMAT _(untranslated_nd_copy_duplicate_format)
#define RD_COPY_DUPLICATE_FORMAT _(untranslated_rd_copy_duplicate_format)
#define TH_COPY_DUPLICATE_FORMAT _(untranslated_th_copy_duplicate_format)

static char *
extract_string_until (const char *original, const char *until_substring)
{
	char *result;
	
	g_assert ((int) strlen (original) >= until_substring - original);
	g_assert (until_substring - original >= 0);

	result = g_malloc (until_substring - original + 1);
	strncpy (result, original, until_substring - original);
	result[until_substring - original] = '\0';
	
	return result;
}

/* Dismantle a file name, separating the base name, the file suffix and removing any
 * (xxxcopy), etc. string. Figure out the count that corresponds to the given
 * (xxxcopy) substring.
 */
static void
parse_previous_duplicate_name (const char *name,
			       char **name_base,
			       const char **suffix,
			       int *count)
{
	const char *tag;

	g_assert (name[0] != '\0');

	*suffix = eel_filename_get_extension_offset (name + 1);

	if (*suffix == NULL || (*suffix)[1] == '\0') {
		/* no suffix */
		*suffix = "";
	}

	tag = strstr (name, COPY_DUPLICATE_TAG);
	if (tag != NULL) {
		if (tag > *suffix) {
			/* handle case "foo. (copy)" */
			*suffix = "";
		}
		*name_base = extract_string_until (name, tag);
		*count = 1;
		return;
	}


	tag = strstr (name, ANOTHER_COPY_DUPLICATE_TAG);
	if (tag != NULL) {
		if (tag > *suffix) {
			/* handle case "foo. (another copy)" */
			*suffix = "";
		}
		*name_base = extract_string_until (name, tag);
		*count = 2;
		return;
	}


	/* Check to see if we got one of st, nd, rd, th. */
	tag = strstr (name, X11TH_COPY_DUPLICATE_TAG);

	if (tag == NULL) {
		tag = strstr (name, X12TH_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, X13TH_COPY_DUPLICATE_TAG);
	}

	if (tag == NULL) {
		tag = strstr (name, ST_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, ND_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, RD_COPY_DUPLICATE_TAG);
	}
	if (tag == NULL) {
		tag = strstr (name, TH_COPY_DUPLICATE_TAG);
	}

	/* If we got one of st, nd, rd, th, fish out the duplicate number. */
	if (tag != NULL) {
		/* localizers: opening parentheses to match the "th copy)" string */
		tag = strstr (name, _(" ("));
		if (tag != NULL) {
			if (tag > *suffix) {
				/* handle case "foo. (22nd copy)" */
				*suffix = "";
			}
			*name_base = extract_string_until (name, tag);
			/* localizers: opening parentheses of the "th copy)" string */
			if (sscanf (tag, _(" (%'d"), count) == 1) {
				if (*count < 1 || *count > 1000000) {
					/* keep the count within a reasonable range */
					*count = 0;
				}
				return;
			}
			*count = 0;
			return;
		}
	}

	
	*count = 0;
	if (**suffix != '\0') {
		*name_base = extract_string_until (name, *suffix);
	} else {
		*name_base = g_strdup (name);
	}
}

static char *
make_next_duplicate_name (const char *base, const char *suffix, int count, int max_length)
{
	const char *format;
	char *result;
	int unshortened_length;
	gboolean use_count;

	if (count < 1) {
		g_warning ("bad count %d in get_duplicate_name", count);
		count = 1;
	}

	if (count <= 2) {

		/* Handle special cases for low numbers.
		 * Perhaps for some locales we will need to add more.
		 */
		switch (count) {
		default:
			g_assert_not_reached ();
			/* fall through */
		case 1:
			format = FIRST_COPY_DUPLICATE_FORMAT;
			break;
		case 2:
			format = SECOND_COPY_DUPLICATE_FORMAT;
			break;

		}

		use_count = FALSE;
	} else {

		/* Handle special cases for the first few numbers of each ten.
		 * For locales where getting this exactly right is difficult,
		 * these can just be made all the same as the general case below.
		 */

		/* Handle special cases for x11th - x20th.
		 */
		switch (count % 100) {
		case 11:
			format = X11TH_COPY_DUPLICATE_FORMAT;
			break;
		case 12:
			format = X12TH_COPY_DUPLICATE_FORMAT;
			break;
		case 13:
			format = X13TH_COPY_DUPLICATE_FORMAT;
			break;
		default:
			format = NULL;
			break;
		}

		if (format == NULL) {
			switch (count % 10) {
			case 1:
				format = ST_COPY_DUPLICATE_FORMAT;
				break;
			case 2:
				format = ND_COPY_DUPLICATE_FORMAT;
				break;
			case 3:
				format = RD_COPY_DUPLICATE_FORMAT;
				break;
			default:
				/* The general case. */
				format = TH_COPY_DUPLICATE_FORMAT;
				break;
			}
		}

		use_count = TRUE;

	}

	if (use_count)
		result = g_strdup_printf (format, base, count, suffix);
	else
		result = g_strdup_printf (format, base, suffix);

	if (max_length > 0 && (unshortened_length = strlen (result)) > max_length) {
		char *new_base;

		new_base = shorten_utf8_string (base, unshortened_length - max_length);
		if (new_base) {
			g_free (result);

			if (use_count)
				result = g_strdup_printf (format, new_base, count, suffix);
			else
				result = g_strdup_printf (format, new_base, suffix);

			g_assert (strlen (result) <= max_length);
			g_free (new_base);
		}
	}

	return result;
}

static char *
get_duplicate_name (const char *name, int count_increment, int max_length)
{
	char *result;
	char *name_base;
	const char *suffix;
	int count;

	parse_previous_duplicate_name (name, &name_base, &suffix, &count);
	result = make_next_duplicate_name (name_base, suffix, count + count_increment, max_length);

	g_free (name_base);

	return result;
}

static gboolean
has_invalid_xml_char (char *str)
{
	gunichar c;

	while (*str != 0) {
		c = g_utf8_get_char (str);
		/* characters XML permits */
		if (!(c == 0x9 ||
		      c == 0xA ||
		      c == 0xD ||
		      (c >= 0x20 && c <= 0xD7FF) ||
		      (c >= 0xE000 && c <= 0xFFFD) ||
		      (c >= 0x10000 && c <= 0x10FFFF))) {
			return TRUE;
		}
		str = g_utf8_next_char (str);
	}
	return FALSE;
}


static char *
custom_full_name_to_string (char *format, va_list va)
{
	GFile *file;
	
	file = va_arg (va, GFile *);
	
	return g_file_get_parse_name (file);
}

static void
custom_full_name_skip (va_list *va)
{
	(void) va_arg (*va, GFile *);
}

static char *
custom_basename_to_string (char *format, va_list va)
{
	GFile *file;
	GFileInfo *info;
	char *name, *basename, *tmp;

	file = va_arg (va, GFile *);

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
				  0,
				  g_cancellable_get_current (),
				  NULL);
	
	name = NULL;
	if (info) {
		name = g_strdup (g_file_info_get_display_name (info));
		g_object_unref (info);
	}
	
	if (name == NULL) {
		basename = g_file_get_basename (file);
		if (g_utf8_validate (basename, -1, NULL)) {
			name = basename;
		} else {
			name = g_uri_escape_string (basename, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
			g_free (basename);
		}
	}

	/* Some chars can't be put in the markup we use for the dialogs... */
	if (has_invalid_xml_char (name)) {
		tmp = name;
		name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
		g_free (tmp);
	}

	/* Finally, if the string is too long, truncate it. */
	if (name != NULL) {
		tmp = name;
		name = eel_str_middle_truncate (tmp, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
		g_free (tmp);
	}

	
	return name;
}

static void
custom_basename_skip (va_list *va)
{
	(void) va_arg (*va, GFile *);
}


static char *
custom_size_to_string (char *format, va_list va)
{
	goffset size;

	size = va_arg (va, goffset);
	return g_format_size (size);
}

static void
custom_size_skip (va_list *va)
{
	(void) va_arg (*va, goffset);
}

static char *
custom_time_to_string (char *format, va_list va)
{
	int secs;

	secs = va_arg (va, int);
	return format_time (secs);
}

static void
custom_time_skip (va_list *va)
{
	(void) va_arg (*va, int);
}

static char *
custom_mount_to_string (char *format, va_list va)
{
	GMount *mount;

	mount = va_arg (va, GMount *);
	return g_mount_get_name (mount);
}

static void
custom_mount_skip (va_list *va)
{
	(void) va_arg (*va, GMount *);
}


static EelPrintfHandler handlers[] = {
	{ 'F', custom_full_name_to_string, custom_full_name_skip },
	{ 'B', custom_basename_to_string, custom_basename_skip },
	{ 'S', custom_size_to_string, custom_size_skip },
	{ 'T', custom_time_to_string, custom_time_skip },
	{ 'V', custom_mount_to_string, custom_mount_skip },
	{ 0 }
};


static char *
f (const char *format, ...) {
	va_list va;
	char *res;
	
	va_start (va, format);
	res = eel_strdup_vprintf_with_custom (handlers, format, va);
	va_end (va);

	return res;
}

#define op_job_new(__type, parent_window) ((__type *)(init_common (sizeof(__type), parent_window)))

static gpointer
init_common (gsize job_size,
	     GtkWindow *parent_window)
{
	CommonJob *common;
	GdkScreen *screen;

	common = g_malloc0 (job_size);

	if (parent_window) {
		common->parent_window = parent_window;
		eel_add_weak_pointer (&common->parent_window);
	}
	common->progress = nemo_progress_info_new ();
	common->cancellable = nemo_progress_info_get_cancellable (common->progress);
	common->time = g_timer_new ();
	common->inhibit_cookie = -1;
	common->screen_num = 0;
	if (parent_window) {
		screen = gtk_widget_get_screen (GTK_WIDGET (parent_window));
		common->screen_num = gdk_screen_get_number (screen);
	}
	
	return common;
}

static void
finalize_common (CommonJob *common)
{
	nemo_progress_info_finish (common->progress);

	if (common->inhibit_cookie != -1) {
		nemo_uninhibit_power_manager (common->inhibit_cookie);
	}

	common->inhibit_cookie = -1;
	g_timer_destroy (common->time);
	eel_remove_weak_pointer (&common->parent_window);
	if (common->skip_files) {
		g_hash_table_destroy (common->skip_files);
	}
	if (common->skip_readdir_error) {
		g_hash_table_destroy (common->skip_readdir_error);
	}

	if (common->undo_info != NULL) {
		nemo_file_undo_manager_set_action (common->undo_info);
		g_object_unref (common->undo_info);
	}

	g_object_unref (common->progress);
	g_object_unref (common->cancellable);
	g_free (common);
}

static void
skip_file (CommonJob *common,
	   GFile *file)
{
	if (common->skip_files == NULL) {
		common->skip_files =
			g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
	}

	g_hash_table_insert (common->skip_files, g_object_ref (file), file);
}

static void
skip_readdir_error (CommonJob *common,
		    GFile *dir)
{
	if (common->skip_readdir_error == NULL) {
		common->skip_readdir_error =
			g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
	}

	g_hash_table_insert (common->skip_readdir_error, g_object_ref (dir), dir);
}

static gboolean
should_skip_file (CommonJob *common,
		  GFile *file)
{
	if (common->skip_files != NULL) {
		return g_hash_table_lookup (common->skip_files, file) != NULL;
	}
	return FALSE;
}

static gboolean
should_skip_readdir_error (CommonJob *common,
			   GFile *dir)
{
	if (common->skip_readdir_error != NULL) {
		return g_hash_table_lookup (common->skip_readdir_error, dir) != NULL;
	}
	return FALSE;
}

static gboolean
can_delete_without_confirm (GFile *file)
{
	if (g_file_has_uri_scheme (file, "burn") ||
	    g_file_has_uri_scheme (file, "x-nemo-desktop")) {
		return TRUE;
	}

	return FALSE;
}

static gboolean
can_delete_files_without_confirm (GList *files)
{
	g_assert (files != NULL);

	while (files != NULL) {
		if (!can_delete_without_confirm (files->data)) {
			return FALSE;
		}

		files = files->next;
	}

	return TRUE;
}

typedef struct {
	GtkWindow **parent_window;
	gboolean ignore_close_box;
	GtkMessageType message_type;
	const char *primary_text;
	const char *secondary_text;
	const char *details_text;
	const char **button_titles;
	gboolean show_all;
	
	int result;
} RunSimpleDialogData;

static gboolean
do_run_simple_dialog (gpointer _data)
{
	RunSimpleDialogData *data = _data;
	const char *button_title;
        GtkWidget *dialog;
	int result;
	int response_id;

	/* Create the dialog. */
	dialog = gtk_message_dialog_new (*data->parent_window,
					 0,
					 data->message_type,
					 GTK_BUTTONS_NONE,
					 NULL);

	g_object_set (dialog,
		      "text", data->primary_text,
		      "secondary-text", data->secondary_text,
		      NULL);

	for (response_id = 0;
	     data->button_titles[response_id] != NULL;
	     response_id++) {
		button_title = data->button_titles[response_id];
		if (!data->show_all && is_all_button_text (button_title)) {
			continue;
		}

		gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, response_id);
		gtk_dialog_set_default_response (GTK_DIALOG (dialog), response_id);
	}

	if (data->details_text) {
		eel_gtk_message_dialog_set_details_label (GTK_MESSAGE_DIALOG (dialog),
							  data->details_text);
	}
	
	/* Run it. */
        result = gtk_dialog_run (GTK_DIALOG (dialog));
	
	while ((result == GTK_RESPONSE_NONE || result == GTK_RESPONSE_DELETE_EVENT) && data->ignore_close_box) {
		result = gtk_dialog_run (GTK_DIALOG (dialog));
	}
	
	gtk_widget_destroy (dialog);

	data->result = result;
	
	return FALSE;
}

/* NOTE: This frees the primary / secondary strings, in order to
   avoid doing that everywhere. So, make sure they are strduped */

static int
run_simple_dialog_va (CommonJob *job,
		      gboolean ignore_close_box,
		      GtkMessageType message_type,
		      char *primary_text,
		      char *secondary_text,
		      const char *details_text,
		      gboolean show_all,
		      va_list varargs)
{
	RunSimpleDialogData *data;
	int res;
	const char *button_title;
	GPtrArray *ptr_array;

	g_timer_stop (job->time);
	
	data = g_new0 (RunSimpleDialogData, 1);
	data->parent_window = &job->parent_window;
	data->ignore_close_box = ignore_close_box;
	data->message_type = message_type;
	data->primary_text = primary_text;
	data->secondary_text = secondary_text;
	data->details_text = details_text;
	data->show_all = show_all;

	ptr_array = g_ptr_array_new ();
	while ((button_title = va_arg (varargs, const char *)) != NULL) {
		g_ptr_array_add (ptr_array, (char *)button_title);
	}
	g_ptr_array_add (ptr_array, NULL);
	data->button_titles = (const char **)g_ptr_array_free (ptr_array, FALSE);

	nemo_progress_info_pause (job->progress);
	g_io_scheduler_job_send_to_mainloop (job->io_job,
					     do_run_simple_dialog,
					     data,
					     NULL);
	nemo_progress_info_resume (job->progress);
	res = data->result;

	g_free (data->button_titles);
	g_free (data);

	g_timer_continue (job->time);

	g_free (primary_text);
	g_free (secondary_text);
	
	return res;
}

#if 0 /* Not used at the moment */
static int
run_simple_dialog (CommonJob *job,
		   gboolean ignore_close_box,
		   GtkMessageType message_type,
		   char *primary_text,
		   char *secondary_text,
		   const char *details_text,
		   ...)
{
	va_list varargs;
	int res;

	va_start (varargs, details_text);
	res = run_simple_dialog_va (job,
				    ignore_close_box,
				    message_type,
				    primary_text,
				    secondary_text,
				    details_text,
				    varargs);
	va_end (varargs);
	return res;
}
#endif

static int
run_error (CommonJob *job,
	   char *primary_text,
	   char *secondary_text,
	   const char *details_text,
	   gboolean show_all,
	   ...)
{
	va_list varargs;
	int res;

	va_start (varargs, show_all);
	res = run_simple_dialog_va (job,
				    FALSE,
				    GTK_MESSAGE_ERROR,
				    primary_text,
				    secondary_text,
				    details_text,
				    show_all,
				    varargs);
	va_end (varargs);
	return res;
}

static int
run_warning (CommonJob *job,
	     char *primary_text,
	     char *secondary_text,
	     const char *details_text,
	     gboolean show_all,
	     ...)
{
	va_list varargs;
	int res;

	va_start (varargs, show_all);
	res = run_simple_dialog_va (job,
				    FALSE,
				    GTK_MESSAGE_WARNING,
				    primary_text,
				    secondary_text,
				    details_text,
				    show_all,
				    varargs);
	va_end (varargs);
	return res;
}

static int
run_question (CommonJob *job,
	      char *primary_text,
	      char *secondary_text,
	      const char *details_text,
	      gboolean show_all,
	      ...)
{
	va_list varargs;
	int res;

	va_start (varargs, show_all);
	res = run_simple_dialog_va (job,
				    FALSE,
				    GTK_MESSAGE_QUESTION,
				    primary_text,
				    secondary_text,
				    details_text,
				    show_all,
				    varargs);
	va_end (varargs);
	return res;
}

static void
inhibit_power_manager (CommonJob *job, const char *message)
{
	job->inhibit_cookie = nemo_inhibit_power_manager (message);
}

static void
abort_job (CommonJob *job)
{
	/* destroy the undo action data too */
	g_clear_object (&job->undo_info);

	g_cancellable_cancel (job->cancellable);
}

static gboolean
job_aborted (CommonJob *job)
{
	return g_cancellable_is_cancelled (job->cancellable);
}

/* Since this happens on a thread we can't use the global prefs object */
static gboolean
should_confirm_trash (void)
{
	gboolean confirm_trash;

	confirm_trash = g_settings_get_boolean (nemo_preferences, NEMO_PREFERENCES_CONFIRM_TRASH);

	return confirm_trash;
}

static gboolean
confirm_delete_from_trash (CommonJob *job,
			   GList *files)
{
	char *prompt;
	int file_count;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!should_confirm_trash ()) {
		return TRUE;
	}

	file_count = g_list_length (files);
	g_assert (file_count > 0);
	
	if (file_count == 1) {
		prompt = f (_("Are you sure you want to permanently delete \"%B\" "
					    "from the trash?"), files->data);
	} else {
		prompt = f (ngettext("Are you sure you want to permanently delete "
				     "the %'d selected item from the trash?",
				     "Are you sure you want to permanently delete "
				     "the %'d selected items from the trash?",
				     file_count), 
			    file_count);
	}

	response = run_warning (job,
				prompt,
				f (_("If you delete an item, it will be permanently lost.")),
				NULL,
				FALSE,
				GTK_STOCK_CANCEL, GTK_STOCK_DELETE,
				NULL);
	
	return (response == 1);
}

static gboolean
confirm_empty_trash (CommonJob *job)
{
	char *prompt;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!should_confirm_trash ()) {
		return TRUE;
	}

	prompt = f (_("Empty all items from Trash?"));

	response = run_warning (job,
				prompt,
				f(_("All items in the Trash will be permanently deleted.")),
				NULL,
				FALSE,
				GTK_STOCK_CANCEL, _("Empty _Trash"),
				NULL);

	return (response == 1);
}

static gboolean
confirm_delete_directly (CommonJob *job,
			 GList *files)
{
	char *prompt;
	int file_count;
	int response;

	/* Just Say Yes if the preference says not to confirm. */
	if (!should_confirm_trash ()) {
		return TRUE;
	}

	file_count = g_list_length (files);
	g_assert (file_count > 0);

	if (can_delete_files_without_confirm (files)) {
		return TRUE;
	}

	if (file_count == 1) {
		prompt = f (_("Are you sure you want to permanently delete \"%B\"?"), 
			    files->data);
	} else {
		prompt = f (ngettext("Are you sure you want to permanently delete "
				     "the %'d selected item?",
				     "Are you sure you want to permanently delete "
				     "the %'d selected items?", file_count),
			    file_count);
	}
	
	response = run_warning (job, 
				prompt,
				f (_("If you delete an item, it will be permanently lost.")),
				NULL,
				FALSE,
				GTK_STOCK_CANCEL, GTK_STOCK_DELETE,
				NULL);

	return response == 1;
}

static void
report_delete_progress (CommonJob *job,
			SourceInfo *source_info,
			TransferInfo *transfer_info)
{
	int files_left;
	double elapsed, transfer_rate;
	int remaining_time;
	gint64 now;
	char *files_left_s;

	now = g_get_monotonic_time ();
	if (transfer_info->last_report_time != 0 &&
	    ABS ((gint64)(transfer_info->last_report_time - now)) < 100 * NSEC_PER_MICROSEC) {
		return;
	}
	transfer_info->last_report_time = now;
	
	files_left = source_info->num_files - transfer_info->num_files;

	/* Races and whatnot could cause this to be negative... */
	if (files_left < 0) {
		files_left = 1;
	}

	files_left_s = f (ngettext ("%'d file left to delete",
				    "%'d files left to delete",
				    files_left),
			  files_left);

	nemo_progress_info_take_status (job->progress,
					    f (_("Deleting files")));

	elapsed = g_timer_elapsed (job->time, NULL);
	if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE) {

		nemo_progress_info_set_details (job->progress, files_left_s);
	} else {
		char *details, *time_left_s;
		transfer_rate = transfer_info->num_files / elapsed;
		remaining_time = files_left / transfer_rate;

		/* To translators: %T will expand to a time like "2 minutes".
 		 * The singular/plural form will be used depending on the remaining time (i.e. the %T argument).
 		 */
		time_left_s = f (ngettext ("%T left",
					   "%T left",
					   seconds_count_format_time_units (remaining_time)),
				 remaining_time);

		details = g_strconcat (files_left_s, "\xE2\x80\x94", time_left_s, NULL);
		nemo_progress_info_take_details (job->progress, details);

		g_free (time_left_s);
	}

	g_free (files_left_s);

	if (source_info->num_files != 0) {
		nemo_progress_info_set_progress (job->progress, transfer_info->num_files, source_info->num_files);
	}
}

static void delete_file (CommonJob *job, GFile *file,
			 gboolean *skipped_file,
			 SourceInfo *source_info,
			 TransferInfo *transfer_info,
			 gboolean toplevel);

static void
delete_dir (CommonJob *job, GFile *dir,
	    gboolean *skipped_file,
	    SourceInfo *source_info,
	    TransferInfo *transfer_info,
	    gboolean toplevel)
{
	GFileInfo *info;
	GError *error;
	GFile *file;
	GFileEnumerator *enumerator;
	char *primary, *secondary, *details;
	int response;
	gboolean skip_error;
	gboolean local_skipped_file;

	local_skipped_file = FALSE;
	
	skip_error = should_skip_readdir_error (job, dir);
 retry:
	error = NULL;
	enumerator = g_file_enumerate_children (dir,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		
		while (!job_aborted (job) &&
		       (info = g_file_enumerator_next_file (enumerator, job->cancellable, skip_error?NULL:&error)) != NULL) {
			file = g_file_get_child (dir,
						 g_file_info_get_name (info));
			delete_file (job, file, &local_skipped_file, source_info, transfer_info, FALSE);
			g_object_unref (file);
			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		g_object_unref (enumerator);
		
		if (error && IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
		} else if (error) {
			primary = f (_("Error while deleting."));
			details = NULL;
			
			if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
				secondary = f (_("Files in the folder \"%B\" cannot be deleted because you do "
						 "not have permissions to see them."), dir);
			} else {
				secondary = f (_("There was an error getting information about the files in the folder \"%B\"."), dir);
				details = error->message;
			}
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						FALSE,
						GTK_STOCK_CANCEL, _("_Skip files"),
						NULL);
			
			g_error_free (error);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 1) {
				/* Skip: Do Nothing */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
		}
		
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	} else {
		primary = f (_("Error while deleting."));
		details = NULL;
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be deleted because you do not have "
					 "permissions to read it."), dir);
		} else {
			secondary = f (_("There was an error reading the folder \"%B\"."), dir);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					FALSE,
					GTK_STOCK_CANCEL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) {
			/* Skip: Do Nothing  */
			local_skipped_file = TRUE;
		} else if (response == 2) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}

	if (!job_aborted (job) &&
	    /* Don't delete dir if there was a skipped file */
	    !local_skipped_file) {
		if (!g_file_delete (dir, job->cancellable, &error)) {
			if (job->skip_all_error) {
				goto skip;
			}
			primary = f (_("Error while deleting."));
			secondary = f (_("Could not remove the folder %B."), dir);
			details = error->message;
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						(source_info->num_files - transfer_info->num_files) > 1,
						GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
						NULL);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 1) { /* skip all */
				job->skip_all_error = TRUE;
				local_skipped_file = TRUE;
			} else if (response == 2) { /* skip */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
			
		skip:
			g_error_free (error);
		} else {
			nemo_file_changes_queue_file_removed (dir);
			transfer_info->num_files ++;
			report_delete_progress (job, source_info, transfer_info);
			return;
		}
	}

	if (local_skipped_file) {
		*skipped_file = TRUE;
	}
}

static void
delete_file (CommonJob *job, GFile *file,
	     gboolean *skipped_file,
	     SourceInfo *source_info,
	     TransferInfo *transfer_info,
	     gboolean toplevel)
{
	GError *error;
	char *primary, *secondary, *details;
	int response;

	if (should_skip_file (job, file)) {
		*skipped_file = TRUE;
		return;
	}
	
	error = NULL;
	if (g_file_delete (file, job->cancellable, &error)) {
		nemo_file_changes_queue_file_removed (file);
		transfer_info->num_files ++;
		report_delete_progress (job, source_info, transfer_info);
		return;
	}

	if (IS_IO_ERROR (error, NOT_EMPTY)) {
		g_error_free (error);
		delete_dir (job, file,
			    skipped_file,
			    source_info, transfer_info,
			    toplevel);
		return;
		
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
		
	} else {
		if (job->skip_all_error) {
			goto skip;
		}
		primary = f (_("Error while deleting."));
		secondary = f (_("There was an error deleting %B."), file);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					(source_info->num_files - transfer_info->num_files) > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	skip:
		g_error_free (error);
	}
	
	*skipped_file = TRUE;
}

static void
delete_files (CommonJob *job, GList *files, int *files_skipped)
{
	GList *l;
	GFile *file;
	SourceInfo source_info;
	TransferInfo transfer_info;
	gboolean skipped_file;
	
	if (job_aborted (job)) {
		return;
	}

	scan_sources (files,
		      &source_info,
		      job,
		      OP_KIND_DELETE);
	if (job_aborted (job)) {
		return;
	}

	g_timer_start (job->time);
	
	memset (&transfer_info, 0, sizeof (transfer_info));
	report_delete_progress (job, &source_info, &transfer_info);
	
	for (l = files;
	     l != NULL && !job_aborted (job);
	     l = l->next) {
		file = l->data;

		skipped_file = FALSE;
		delete_file (job, file,
			     &skipped_file,
			     &source_info, &transfer_info,
			     TRUE);
		if (skipped_file) {
			(*files_skipped)++;
		}
	}
}

static void
report_trash_progress (CommonJob *job,
		       int files_trashed,
		       int total_files)
{
	int files_left;
	char *s;

	files_left = total_files - files_trashed;

	nemo_progress_info_take_status (job->progress,
					    f (_("Moving files to trash")));

	s = f (ngettext ("%'d file left to trash",
			 "%'d files left to trash",
			 files_left),
	       files_left);
	nemo_progress_info_take_details (job->progress, s);

	if (total_files != 0) {
		nemo_progress_info_set_progress (job->progress, files_trashed, total_files);
	}
}


static void
trash_files (CommonJob *job, GList *files, int *files_skipped)
{
	GList *l;
	GFile *file;
	GList *to_delete;
	GError *error;
	int total_files, files_trashed;
	char *primary, *secondary, *details;
	int response;

	if (job_aborted (job)) {
		return;
	}

	total_files = g_list_length (files);
	files_trashed = 0;

	report_trash_progress (job, files_trashed, total_files);

	to_delete = NULL;
	for (l = files;
	     l != NULL && !job_aborted (job);
	     l = l->next) {
		file = l->data;

		error = NULL;

		if (!g_file_trash (file, job->cancellable, &error)) {
			if (job->skip_all_error) {
				(*files_skipped)++;
				goto skip;
			}

			if (job->delete_all) {
				to_delete = g_list_prepend (to_delete, file);
				goto skip;
			}

			primary = f (_("Cannot move file to trash, do you want to delete immediately?"));
			secondary = f (_("The file \"%B\" cannot be moved to the trash."), file);
			details = NULL;
			if (!IS_IO_ERROR (error, NOT_SUPPORTED)) {
				details = error->message;
			}

			response = run_question (job,
						 primary,
						 secondary,
						 details,
						 (total_files - files_trashed) > 1,
						 GTK_STOCK_CANCEL, SKIP_ALL, SKIP, DELETE_ALL, GTK_STOCK_DELETE,
						 NULL);

			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				((DeleteJob *) job)->user_cancel = TRUE;				
				abort_job (job);
			} else if (response == 1) { /* skip all */
				(*files_skipped)++;
				job->skip_all_error = TRUE;
			} else if (response == 2) { /* skip */
				(*files_skipped)++;
			} else if (response == 3) { /* delete all */
				to_delete = g_list_prepend (to_delete, file);
				job->delete_all = TRUE;
			} else if (response == 4) { /* delete */
				to_delete = g_list_prepend (to_delete, file);
			}

		skip:
			g_error_free (error);
			total_files--;
		} else {
			nemo_file_changes_queue_file_removed (file);

			if (job->undo_info != NULL) {
				nemo_file_undo_info_trash_add_file (NEMO_FILE_UNDO_INFO_TRASH (job->undo_info), file);
			}

			files_trashed++;
			report_trash_progress (job, files_trashed, total_files);
		}
	}

	if (to_delete) {
		to_delete = g_list_reverse (to_delete);
		delete_files (job, to_delete, files_skipped);
		g_list_free (to_delete);
	}
}

static gboolean
delete_job_done (gpointer user_data)
{
	DeleteJob *job;
	GHashTable *debuting_uris;

	job = user_data;

	g_list_free_full (job->files, g_object_unref);

	if (job->done_callback) {
		debuting_uris = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);
		job->done_callback (debuting_uris, job->user_cancel, job->done_callback_data);
		g_hash_table_unref (debuting_uris);
	}
	
	finalize_common ((CommonJob *)job);

	nemo_file_changes_consume_changes (TRUE);

	return FALSE;
}

static gboolean
delete_job (GIOSchedulerJob *io_job,
	    GCancellable *cancellable,
	    gpointer user_data)
{
	DeleteJob *job = user_data;
	GList *to_trash_files;
	GList *to_delete_files;
	GList *l;
	GFile *file;
	gboolean confirmed;
	CommonJob *common;
	gboolean must_confirm_delete_in_trash;
	gboolean must_confirm_delete;
	int files_skipped;

	common = (CommonJob *)job;
	common->io_job = io_job;

	nemo_progress_info_start (job->common.progress);
	
	to_trash_files = NULL;
	to_delete_files = NULL;

	must_confirm_delete_in_trash = FALSE;
	must_confirm_delete = FALSE;
	files_skipped = 0;
	
	for (l = job->files; l != NULL; l = l->next) {
		file = l->data;
		
		if (job->try_trash &&
		    g_file_has_uri_scheme (file, "trash")) {
			must_confirm_delete_in_trash = TRUE;
			to_delete_files = g_list_prepend (to_delete_files, file);
		} else if (can_delete_without_confirm (file)) {
			to_delete_files = g_list_prepend (to_delete_files, file);
		} else {
			if (job->try_trash) {
				to_trash_files = g_list_prepend (to_trash_files, file);
			} else {
				must_confirm_delete = TRUE;
				to_delete_files = g_list_prepend (to_delete_files, file);
			}
		}
	}
	
	if (to_delete_files != NULL) {
		to_delete_files = g_list_reverse (to_delete_files);
		confirmed = TRUE;
		if (must_confirm_delete_in_trash) {
			confirmed = confirm_delete_from_trash (common, to_delete_files);
		} else if (must_confirm_delete) {
			confirmed = confirm_delete_directly (common, to_delete_files);
		}
		if (confirmed) {
			delete_files (common, to_delete_files, &files_skipped);
		} else {
			job->user_cancel = TRUE;
		}
	}
	
	if (to_trash_files != NULL) {
		to_trash_files = g_list_reverse (to_trash_files);
		
		trash_files (common, to_trash_files, &files_skipped);
	}
	
	g_list_free (to_trash_files);
	g_list_free (to_delete_files);
	
	if (files_skipped == g_list_length (job->files)) {
		/* User has skipped all files, report user cancel */
		job->user_cancel = TRUE;
	}

	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   delete_job_done,
						   job,
						   NULL);

	return FALSE;
}

static void
trash_or_delete_internal (GList                  *files,
			  GtkWindow              *parent_window,
			  gboolean                try_trash,			  
			  NemoDeleteCallback  done_callback,
			  gpointer                done_callback_data)
{
	DeleteJob *job;

	/* TODO: special case desktop icon link files ... */

	job = op_job_new (DeleteJob, parent_window);
	job->files = eel_g_object_list_copy (files);
	job->try_trash = try_trash;
	job->user_cancel = FALSE;
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;

	if (try_trash) {
		inhibit_power_manager ((CommonJob *)job, _("Trashing Files"));
	} else {
		inhibit_power_manager ((CommonJob *)job, _("Deleting Files"));
	}
	
	if (try_trash && !nemo_file_undo_manager_pop_flag ()) {
		job->common.undo_info = nemo_file_undo_info_trash_new (g_list_length (files));
	}

	g_io_scheduler_push_job (delete_job,
			   job,
			   NULL,
			   0,
			   NULL);
}

void
nemo_file_operations_trash_or_delete (GList                  *files,
					  GtkWindow              *parent_window,
					  NemoDeleteCallback  done_callback,
					  gpointer                done_callback_data)
{
	trash_or_delete_internal (files, parent_window,
				  TRUE,
				  done_callback,  done_callback_data);
}

void
nemo_file_operations_delete (GList                  *files, 
				 GtkWindow              *parent_window,
				 NemoDeleteCallback  done_callback,
				 gpointer                done_callback_data)
{
	trash_or_delete_internal (files, parent_window,
				  FALSE,			  
				  done_callback,  done_callback_data);
}



typedef struct {
	gboolean eject;
	GMount *mount;
	GtkWindow *parent_window;
	NemoUnmountCallback callback;
	gpointer callback_data;
} UnmountData;

static void
unmount_mount_callback (GObject *source_object,
			GAsyncResult *res,
			gpointer user_data)
{
	UnmountData *data = user_data;
	GError *error;
	char *primary;
	gboolean unmounted;

	error = NULL;
	if (data->eject) {
		unmounted = g_mount_eject_with_operation_finish (G_MOUNT (source_object),
								 res, &error);
	} else {
		unmounted = g_mount_unmount_with_operation_finish (G_MOUNT (source_object),
								   res, &error);
	}
	
	if (! unmounted) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED) {
			if (data->eject) {
				primary = f (_("Unable to eject %V"), source_object);
			} else {
				primary = f (_("Unable to unmount %V"), source_object);
			}
			eel_show_error_dialog (primary,
					       error->message,
					       data->parent_window);
			g_free (primary);
		}
	}

	if (data->callback) {
		data->callback (data->callback_data);
	}

	if (error != NULL) {
		g_error_free (error);
	}
	
	eel_remove_weak_pointer (&data->parent_window);
	g_object_unref (data->mount);
	g_free (data);
}

static void
do_unmount (UnmountData *data)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (data->parent_window);
	if (data->eject) {
		g_mount_eject_with_operation (data->mount,
					      0,
					      mount_op,
					      NULL,
					      unmount_mount_callback,
					      data);
	} else {
		g_mount_unmount_with_operation (data->mount,
						0,
						mount_op,
						NULL,
						unmount_mount_callback,
						data);
	}
	g_object_unref (mount_op);
}

static gboolean
dir_has_files (GFile *dir)
{
	GFileEnumerator *enumerator;
	gboolean res;
	GFileInfo *file_info;

	res = FALSE;
	
	enumerator = g_file_enumerate_children (dir,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						0,
						NULL, NULL);
	if (enumerator) {
		file_info = g_file_enumerator_next_file (enumerator, NULL, NULL);
		if (file_info != NULL) {
			res = TRUE;
			g_object_unref (file_info);
		}
		
		g_file_enumerator_close (enumerator, NULL, NULL);
		g_object_unref (enumerator);
	}
	

	return res;
}

static GList *
get_trash_dirs_for_mount (GMount *mount)
{
	GFile *root;
	GFile *trash;
	char *relpath;
	GList *list;

	root = g_mount_get_root (mount);
	if (root == NULL) {
		return NULL;
	}

	list = NULL;
	
	if (g_file_is_native (root)) {
		relpath = g_strdup_printf (".Trash/%d", getuid ());
		trash = g_file_resolve_relative_path (root, relpath);
		g_free (relpath);

		list = g_list_prepend (list, g_file_get_child (trash, "files"));
		list = g_list_prepend (list, g_file_get_child (trash, "info"));
		
		g_object_unref (trash);
		
		relpath = g_strdup_printf (".Trash-%d", getuid ());
		trash = g_file_get_child (root, relpath);
		g_free (relpath);

		list = g_list_prepend (list, g_file_get_child (trash, "files"));
		list = g_list_prepend (list, g_file_get_child (trash, "info"));
		
		g_object_unref (trash);
	}
	
	g_object_unref (root);
	
	return list;
}

static gboolean
has_trash_files (GMount *mount)
{
	GList *dirs, *l;
	GFile *dir;
	gboolean res;

	dirs = get_trash_dirs_for_mount (mount);

	res = FALSE;

	for (l = dirs; l != NULL; l = l->next) {
		dir = l->data;

		if (dir_has_files (dir)) {
			res = TRUE;
			break;
		}
	}

	g_list_free_full (dirs, g_object_unref);
	
	return res;
}


static gint
prompt_empty_trash (GtkWindow *parent_window)
{
	gint                    result;
	GtkWidget               *dialog;
	GdkScreen               *screen;

	screen = NULL;
	if (parent_window != NULL) {
		screen = gtk_widget_get_screen (GTK_WIDGET (parent_window));
	}

	/* Do we need to be modal ? */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Do you want to empty the trash before you unmount?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  _("In order to regain the "
						    "free space on this volume "
						    "the trash must be emptied. "
						    "All trashed items on the volume "
						    "will be permanently lost."));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), 
	                        _("Do _not Empty Trash"), GTK_RESPONSE_REJECT, 
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, 
	                        _("Empty _Trash"), GTK_RESPONSE_ACCEPT, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_title (GTK_WINDOW (dialog), ""); /* as per HIG */
	gtk_window_set_skip_taskbar_hint (GTK_WINDOW (dialog), TRUE);
	if (screen) {
		gtk_window_set_screen (GTK_WINDOW (dialog), screen);
	}
	atk_object_set_role (gtk_widget_get_accessible (dialog), ATK_ROLE_ALERT);
	gtk_window_set_wmclass (GTK_WINDOW (dialog), "empty_trash",
				"Nemo");
	
	/* Make transient for the window group */
	gtk_widget_realize (dialog);
	if (screen != NULL) {
		gdk_window_set_transient_for (gtk_widget_get_window (GTK_WIDGET (dialog)),
				      		gdk_screen_get_root_window (screen));
	}
	
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return result;
}

static void
empty_trash_for_unmount_done (gboolean success,
			      gpointer user_data)
{
	UnmountData *data = user_data;
	do_unmount (data);
}

void
nemo_file_operations_unmount_mount_full (GtkWindow                      *parent_window,
					     GMount                         *mount,
					     gboolean                        eject,
					     gboolean                        check_trash,
					     NemoUnmountCallback         callback,
					     gpointer                        callback_data)
{
	UnmountData *data;
	int response;

	data = g_new0 (UnmountData, 1);
	data->callback = callback;
	data->callback_data = callback_data;
	if (parent_window) {
		data->parent_window = parent_window;
		eel_add_weak_pointer (&data->parent_window);
		
	}
	data->eject = eject;
	data->mount = g_object_ref (mount);

	if (check_trash && has_trash_files (mount)) {
		response = prompt_empty_trash (parent_window);

		if (response == GTK_RESPONSE_ACCEPT) {
			EmptyTrashJob *job;
			
			job = op_job_new (EmptyTrashJob, parent_window);
			job->should_confirm = FALSE;
			job->trash_dirs = get_trash_dirs_for_mount (mount);
			job->done_callback = empty_trash_for_unmount_done;
			job->done_callback_data = data;
			g_io_scheduler_push_job (empty_trash_job,
					   job,
					   NULL,
					   0,
					   NULL);
			return;
		} else if (response == GTK_RESPONSE_CANCEL) {
			if (callback) {
				callback (callback_data);
			}
			eel_remove_weak_pointer (&data->parent_window);
			g_object_unref (data->mount);
			g_free (data);
			return;
		}
	}
	
	do_unmount (data);
}

void
nemo_file_operations_unmount_mount (GtkWindow                      *parent_window,
					GMount                         *mount,
					gboolean                        eject,
					gboolean                        check_trash)
{
	nemo_file_operations_unmount_mount_full (parent_window, mount, eject,
						     check_trash, NULL, NULL);
}

static void
mount_callback_data_notify (gpointer data,
			    GObject *object)
{
	GMountOperation *mount_op;

	mount_op = G_MOUNT_OPERATION (data);
	g_object_set_data (G_OBJECT (mount_op), "mount-callback", NULL);
	g_object_set_data (G_OBJECT (mount_op), "mount-callback-data", NULL);
}

static void
volume_mount_cb (GObject *source_object,
		 GAsyncResult *res,
		 gpointer user_data)
{
	NemoMountCallback mount_callback;
	GObject *mount_callback_data_object;
	GMountOperation *mount_op = user_data;
	GError *error;
	char *primary;
	char *name;
	gboolean success;

	success = TRUE;
	error = NULL;
	if (!g_volume_mount_finish (G_VOLUME (source_object), res, &error)) {
		if (error->code != G_IO_ERROR_FAILED_HANDLED &&
                    error->code != G_IO_ERROR_ALREADY_MOUNTED) {
			name = g_volume_get_name (G_VOLUME (source_object));
			primary = g_strdup_printf (_("Unable to mount %s"), name);
			g_free (name);
			success = FALSE;
			eel_show_error_dialog (primary,
					       error->message,
					       NULL);
			g_free (primary);
		}
		g_error_free (error);
	}

	mount_callback = (NemoMountCallback)
		g_object_get_data (G_OBJECT (mount_op), "mount-callback");
	mount_callback_data_object =
		g_object_get_data (G_OBJECT (mount_op), "mount-callback-data");

	if (mount_callback != NULL) {
		(* mount_callback) (G_VOLUME (source_object),
				    success,
				    mount_callback_data_object);

	    	if (mount_callback_data_object != NULL) {
			g_object_weak_unref (mount_callback_data_object,
					     mount_callback_data_notify,
					     mount_op);
		}
	}

	g_object_unref (mount_op);
}


void
nemo_file_operations_mount_volume (GtkWindow *parent_window,
				       GVolume *volume)
{
	nemo_file_operations_mount_volume_full (parent_window, volume,
						    NULL, NULL);
}

void
nemo_file_operations_mount_volume_full (GtkWindow *parent_window,
					    GVolume *volume,
					    NemoMountCallback mount_callback,
					    GObject *mount_callback_data_object)
{
	GMountOperation *mount_op;

	mount_op = gtk_mount_operation_new (parent_window);
	g_mount_operation_set_password_save (mount_op, G_PASSWORD_SAVE_FOR_SESSION);
	g_object_set_data (G_OBJECT (mount_op),
			   "mount-callback",
			   mount_callback);

	if (mount_callback != NULL &&
	    mount_callback_data_object != NULL) {
		g_object_weak_ref (mount_callback_data_object,
				   mount_callback_data_notify,
				   mount_op);
	}
	g_object_set_data (G_OBJECT (mount_op),
			   "mount-callback-data",
			   mount_callback_data_object);

	g_volume_mount (volume, 0, mount_op, NULL, volume_mount_cb, mount_op);
}

static void
report_count_progress (CommonJob *job,
		       SourceInfo *source_info)
{
	char *s;

	switch (source_info->op) {
	default:
	case OP_KIND_COPY:
		s = f (ngettext("Preparing to copy %'d file (%S)",
		                "Preparing to copy %'d files (%S)",
		                source_info->num_files),
		       source_info->num_files, source_info->num_bytes);
		break;
	case OP_KIND_MOVE:
		s = f (ngettext("Preparing to move %'d file (%S)",
		                "Preparing to move %'d files (%S)",
		                source_info->num_files),
		       source_info->num_files, source_info->num_bytes);
		break;
	case OP_KIND_DELETE:
		s = f (ngettext("Preparing to delete %'d file (%S)",
		                "Preparing to delete %'d files (%S)",
		                source_info->num_files),
		       source_info->num_files, source_info->num_bytes);
		break;
	case OP_KIND_TRASH:
		s = f (ngettext("Preparing to trash %'d file",
		                "Preparing to trash %'d files",
		                source_info->num_files),
		       source_info->num_files);
		break;
	} 

	nemo_progress_info_take_details (job->progress, s);
	nemo_progress_info_pulse_progress (job->progress);
}

static void
count_file (GFileInfo *info,
	    CommonJob *job,
	    SourceInfo *source_info)
{
	source_info->num_files += 1;
	source_info->num_bytes += g_file_info_get_size (info);

	if (source_info->num_files_since_progress++ > 100) {
		report_count_progress (job, source_info);
		source_info->num_files_since_progress = 0;
	}
}

static char *
get_scan_primary (OpKind kind)
{
	switch (kind) {
	default:
	case OP_KIND_COPY:
		return f (_("Error while copying."));
	case OP_KIND_MOVE:
		return f (_("Error while moving."));
	case OP_KIND_DELETE:
		return f (_("Error while deleting."));
	case OP_KIND_TRASH:
		return f (_("Error while moving files to trash."));
	}
}

static void
scan_dir (GFile *dir,
	  SourceInfo *source_info,
	  CommonJob *job,
	  GQueue *dirs)
{
	GFileInfo *info;
	GError *error;
	GFile *subdir;
	GFileEnumerator *enumerator;
	char *primary, *secondary, *details;
	int response;
	SourceInfo saved_info;

	saved_info = *source_info;

 retry:
	error = NULL;
	enumerator = g_file_enumerate_children (dir,
						G_FILE_ATTRIBUTE_STANDARD_NAME","
						G_FILE_ATTRIBUTE_STANDARD_TYPE","
						G_FILE_ATTRIBUTE_STANDARD_SIZE,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		while ((info = g_file_enumerator_next_file (enumerator, job->cancellable, &error)) != NULL) {
			count_file (info, job, source_info);

			if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
				subdir = g_file_get_child (dir,
							   g_file_info_get_name (info));
				
				/* Push to head, since we want depth-first */
				g_queue_push_head (dirs, subdir);
			}

			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		g_object_unref (enumerator);
		
		if (error && IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
		} else if (error) {
			primary = get_scan_primary (source_info->op);
			details = NULL;
			
			if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
				secondary = f (_("Files in the folder \"%B\" cannot be handled because you do "
						 "not have permissions to see them."), dir);
			} else {
				secondary = f (_("There was an error getting information about the files in the folder \"%B\"."), dir);
				details = error->message;
			}
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						FALSE,
						GTK_STOCK_CANCEL, RETRY, SKIP,
						NULL);

			g_error_free (error);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 1) {
				*source_info = saved_info;
				goto retry;
			} else if (response == 2) {
				skip_readdir_error (job, dir);
			} else {
				g_assert_not_reached ();
			}
		}
		
	} else if (job->skip_all_error) {
		g_error_free (error);
		skip_file (job, dir);
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	} else {	
		primary = get_scan_primary (source_info->op);
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be handled because you do not have "
					 "permissions to read it."), dir);
		} else {
			secondary = f (_("There was an error reading the folder \"%B\"."), dir);
			details = error->message;
		}
		/* set show_all to TRUE here, as we don't know how many
		 * files we'll end up processing yet.
		 */
		response = run_warning (job,
					primary,
					secondary,
					details,
					TRUE,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP, RETRY,
					NULL);
		
		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1 || response == 2) {
			if (response == 1) {
				job->skip_all_error = TRUE;
			}
			skip_file (job, dir);
		} else if (response == 3) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
}	

static void
scan_file (GFile *file,
	   SourceInfo *source_info,
	   CommonJob *job)
{
	GFileInfo *info;
	GError *error;
	GQueue *dirs;
	GFile *dir;
	char *primary;
	char *secondary;
	char *details;
	int response;

	dirs = g_queue_new ();
	
 retry:
	error = NULL;
	info = g_file_query_info (file, 
				  G_FILE_ATTRIBUTE_STANDARD_TYPE","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  job->cancellable,
				  &error);

	if (info) {
		count_file (info, job, source_info);

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			g_queue_push_head (dirs, g_object_ref (file));
		}
		
		g_object_unref (info);
	} else if (job->skip_all_error) {
		g_error_free (error);
		skip_file (job, file);
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	} else {
		primary = get_scan_primary (source_info->op);
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The file \"%B\" cannot be handled because you do not have "
					 "permissions to read it."), file);
		} else {
			secondary = f (_("There was an error getting information about \"%B\"."), file);
			details = error->message;
		}
		/* set show_all to TRUE here, as we don't know how many
		 * files we'll end up processing yet.
		 */
		response = run_warning (job,
					primary,
					secondary,
					details,
					TRUE,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP, RETRY,
					NULL);
		
		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1 || response == 2) {
			if (response == 1) {
				job->skip_all_error = TRUE;
			}
			skip_file (job, file);
		} else if (response == 3) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
		
	while (!job_aborted (job) && 
	       (dir = g_queue_pop_head (dirs)) != NULL) {
		scan_dir (dir, source_info, job, dirs);
		g_object_unref (dir);
	}

	/* Free all from queue if we exited early */
	g_queue_foreach (dirs, (GFunc)g_object_unref, NULL);
	g_queue_free (dirs);
}

static void
scan_sources (GList *files,
	      SourceInfo *source_info,
	      CommonJob *job,
	      OpKind kind)
{
	GList *l;
	GFile *file;

	memset (source_info, 0, sizeof (SourceInfo));
	source_info->op = kind;

	report_count_progress (job, source_info);
	
	for (l = files; l != NULL && !job_aborted (job); l = l->next) {
		file = l->data;

		scan_file (file,
			   source_info,
			   job);
	}

	/* Make sure we report the final count */
	report_count_progress (job, source_info);
}

static void
verify_destination (CommonJob *job,
		    GFile *dest,
		    char **dest_fs_id,
		    goffset required_size)
{
	GFileInfo *info, *fsinfo;
	GError *error;
	guint64 free_size;
	guint64 size_difference;
	char *primary, *secondary, *details;
	int response;
	GFileType file_type;

	if (dest_fs_id) {
		*dest_fs_id = NULL;
	}

 retry:
	
	error = NULL;
	info = g_file_query_info (dest, 
				  G_FILE_ATTRIBUTE_STANDARD_TYPE","
				  G_FILE_ATTRIBUTE_ID_FILESYSTEM,
				  0,
				  job->cancellable,
				  &error);

	if (info == NULL) {
		if (IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
			return;
		}
		
		primary = f (_("Error while copying to \"%B\"."), dest);
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("You do not have permissions to access the destination folder."));
		} else {
			secondary = f (_("There was an error getting information about the destination."));
			details = error->message;
		}

		response = run_error (job,
				      primary,
				      secondary,
				      details,
				      FALSE,
				      GTK_STOCK_CANCEL, RETRY,
				      NULL);
		
		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}

		return;
	}

	file_type = g_file_info_get_file_type (info);

	if (dest_fs_id) {
		*dest_fs_id =
			g_strdup (g_file_info_get_attribute_string (info,
								    G_FILE_ATTRIBUTE_ID_FILESYSTEM));
	}
	
	g_object_unref (info);
	
	if (file_type != G_FILE_TYPE_DIRECTORY) {
		primary = f (_("Error while copying to \"%B\"."), dest);
		secondary = f (_("The destination is not a folder."));

		response = run_error (job,
				      primary,
				      secondary,
				      NULL,
				      FALSE,
				      GTK_STOCK_CANCEL,
				      NULL);
		
		abort_job (job);
		return;
	}
	
	fsinfo = g_file_query_filesystem_info (dest,
					       G_FILE_ATTRIBUTE_FILESYSTEM_FREE","
					       G_FILE_ATTRIBUTE_FILESYSTEM_READONLY,
					       job->cancellable,
					       NULL);
	if (fsinfo == NULL) {
		/* All sorts of things can go wrong getting the fs info (like not supported)
		 * only check these things if the fs returns them
		 */
		return;
	}
	
	if (required_size > 0 &&
	    g_file_info_has_attribute (fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_FREE)) {
		free_size = g_file_info_get_attribute_uint64 (fsinfo,
							      G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
		
		if (free_size < required_size) {
			size_difference = required_size - free_size;
			primary = f (_("Error while copying to \"%B\"."), dest);
			secondary = f (_("There is not enough space on the destination. Try to remove files to make space."));
			
			details = f (_("%S more space is required to copy to the destination."), size_difference);
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						FALSE,
						GTK_STOCK_CANCEL,
						COPY_FORCE,
						RETRY,
						NULL);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 2) {
				goto retry;
			} else if (response == 1) {
				/* We are forced to copy - just fall through ... */
			} else {
				g_assert_not_reached ();
			}
		}
	}
	
	if (!job_aborted (job) &&
	    g_file_info_get_attribute_boolean (fsinfo,
					       G_FILE_ATTRIBUTE_FILESYSTEM_READONLY)) {
		primary = f (_("Error while copying to \"%B\"."), dest);
		secondary = f (_("The destination is read-only."));

		response = run_error (job,
				      primary,
				      secondary,
				      NULL,
				      FALSE,
				      GTK_STOCK_CANCEL,
				      NULL);
		
		g_error_free (error);

		abort_job (job);
	}
	
	g_object_unref (fsinfo);
}

static void
report_copy_progress (CopyMoveJob *copy_job,
		      SourceInfo *source_info,
		      TransferInfo *transfer_info)
{
	int files_left;
	goffset total_size;
	double elapsed, transfer_rate;
	int remaining_time;
	guint64 now;
	CommonJob *job;
	gboolean is_move;

	job = (CommonJob *)copy_job;

	is_move = copy_job->is_move;
	
	now = g_get_monotonic_time ();

	if (transfer_info->last_report_time != 0 &&
	    ABS ((gint64)(transfer_info->last_report_time - now)) < 100 * NSEC_PER_MICROSEC) {
		return;
	}
	transfer_info->last_report_time = now;
	
	files_left = source_info->num_files - transfer_info->num_files;

	/* Races and whatnot could cause this to be negative... */
	if (files_left < 0) {
		files_left = 1;
	}

	if (files_left != transfer_info->last_reported_files_left ||
	    transfer_info->last_reported_files_left == 0) {
		/* Avoid changing this unless files_left changed since last time */
		transfer_info->last_reported_files_left = files_left;
		
		if (source_info->num_files == 1) {
			if (copy_job->destination != NULL) {
				nemo_progress_info_take_status (job->progress,
								    f (is_move ?
								       _("Moving \"%B\" to \"%B\""):
								       _("Copying \"%B\" to \"%B\""),
								       copy_job->fake_display_source != NULL ?
								       copy_job->fake_display_source :
								       (GFile *)copy_job->files->data,
								       copy_job->destination));
			} else {
				nemo_progress_info_take_status (job->progress,
								    f (_("Duplicating \"%B\""),
								       (GFile *)copy_job->files->data));
			}
		} else if (copy_job->files != NULL &&
			   copy_job->files->next == NULL) {
			if (copy_job->destination != NULL) {
				nemo_progress_info_take_status (job->progress,
								    f (is_move ?
								       _("Moving file %'d of %'d (in \"%B\") to \"%B\"")
								       :
								       _("Copying file %'d of %'d (in \"%B\") to \"%B\""),
								       transfer_info->num_files + 1,
								       source_info->num_files,
								       (GFile *)copy_job->files->data,
								       copy_job->destination));
			} else {
				nemo_progress_info_take_status (job->progress,
								    f (_("Duplicating file %'d of %'d (in \"%B\")"),
								       transfer_info->num_files + 1,
								       source_info->num_files,
								       (GFile *)copy_job->files->data));
			}
		} else {
			if (copy_job->destination != NULL) {
				nemo_progress_info_take_status (job->progress,
								    f (is_move ?
								       _("Moving file %'d of %'d to \"%B\"")
								       :
								       _ ("Copying file %'d of %'d to \"%B\""),
								       transfer_info->num_files + 1,
								       source_info->num_files,
								       copy_job->destination));
			} else {
				nemo_progress_info_take_status (job->progress,
								    f (_("Duplicating file %'d of %'d"),
								       transfer_info->num_files + 1,
								       source_info->num_files));
			}
		}
	}
	
	total_size = MAX (source_info->num_bytes, transfer_info->num_bytes);
	
	elapsed = g_timer_elapsed (job->time, NULL);
	transfer_rate = 0;
	if (elapsed > 0) {
		transfer_rate = transfer_info->num_bytes / elapsed;
	}

	if (elapsed < SECONDS_NEEDED_FOR_RELIABLE_TRANSFER_RATE &&
	    transfer_rate > 0) {
		char *s;
		/* To translators: %S will expand to a size like "2 bytes" or "3 MB", so something like "4 kb of 4 MB" */		
		s = f (_("%S of %S"), transfer_info->num_bytes, total_size);
		nemo_progress_info_take_details (job->progress, s);
	} else {
		char *s;
		remaining_time = (total_size - transfer_info->num_bytes) / transfer_rate;

		/* To translators: %S will expand to a size like "2 bytes" or "3 MB", %T to a time duration like
		 * "2 minutes". So the whole thing will be something like "2 kb of 4 MB -- 2 hours left (4kb/sec)"
		 *
		 * The singular/plural form will be used depending on the remaining time (i.e. the %T argument).
		 */		
		s = f (ngettext ("%S of %S \xE2\x80\x94 %T left (%S/sec)",
				 "%S of %S \xE2\x80\x94 %T left (%S/sec)",
				 seconds_count_format_time_units (remaining_time)),
		       transfer_info->num_bytes, total_size,
		       remaining_time,
		       (goffset)transfer_rate);
		nemo_progress_info_take_details (job->progress, s);
	}

	nemo_progress_info_set_progress (job->progress, transfer_info->num_bytes, total_size);
}

static int
get_max_name_length (GFile *file_dir)
{
	int max_length;
	char *dir;
	long max_path;
	long max_name;

	max_length = -1;

	if (!g_file_has_uri_scheme (file_dir, "file"))
		return max_length;

	dir = g_file_get_path (file_dir);
	if (!dir)
		return max_length;

	max_path = pathconf (dir, _PC_PATH_MAX);
	max_name = pathconf (dir, _PC_NAME_MAX);

	if (max_name == -1 && max_path == -1) {
		max_length = -1;
	} else if (max_name == -1 && max_path != -1) {
		max_length = max_path - (strlen (dir) + 1);
	} else if (max_name != -1 && max_path == -1) {
		max_length = max_name;
	} else {
		int leftover;

		leftover = max_path - (strlen (dir) + 1);

		max_length = MIN (leftover, max_name);
	}

	g_free (dir);

	return max_length;
}

#define FAT_FORBIDDEN_CHARACTERS "/:;*?\"<>"

static gboolean
str_replace (char *str,
	     const char *chars_to_replace,
	     char replacement)
{
	gboolean success;
	int i;

	success = FALSE;
	for (i = 0; str[i] != '\0'; i++) {
		if (strchr (chars_to_replace, str[i])) {
			success = TRUE;
			str[i] = replacement;
		}
	}

	return success;
}

static gboolean
make_file_name_valid_for_dest_fs (char *filename,
				 const char *dest_fs_type)
{
	if (dest_fs_type != NULL && filename != NULL) {
		if (!strcmp (dest_fs_type, "fat")  ||
		    !strcmp (dest_fs_type, "vfat") ||
		    !strcmp (dest_fs_type, "msdos") ||
		    !strcmp (dest_fs_type, "msdosfs")) {
			gboolean ret;
			int i, old_len;

			ret = str_replace (filename, FAT_FORBIDDEN_CHARACTERS, '_');

			old_len = strlen (filename);
			for (i = 0; i < old_len; i++) {
				if (filename[i] != ' ') {
					g_strchomp (filename);
					ret |= (old_len != strlen (filename));
					break;
				}
			}

			return ret;
		}
	}

	return FALSE;
}

static GFile *
get_unique_target_file (GFile *src,
			GFile *dest_dir,
			gboolean same_fs,
			const char *dest_fs_type,
			int count)
{
	const char *editname, *end;
	char *basename, *new_name;
	GFileInfo *info;
	GFile *dest;
	int max_length;

	max_length = get_max_name_length (dest_dir);
	
	dest = NULL;
	info = g_file_query_info (src,
				  G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME,
				  0, NULL, NULL);
	if (info != NULL) {
		editname = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
		
		if (editname != NULL) {
			new_name = get_duplicate_name (editname, count, max_length);
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
			g_free (new_name);
		}
		
		g_object_unref (info);
	}

	if (dest == NULL) {
		basename = g_file_get_basename (src);

		if (g_utf8_validate (basename, -1, NULL)) {
			new_name = get_duplicate_name (basename, count, max_length);
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
			g_free (new_name);
		} 

		if (dest == NULL) {
			end = strrchr (basename, '.');
			if (end != NULL) {
				count += atoi (end + 1);
			}
			new_name = g_strdup_printf ("%s.%d", basename, count);
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child (dest_dir, new_name);
			g_free (new_name);
		}
		
		g_free (basename);
	}

	return dest;
}

static GFile *
get_target_file_for_link (GFile *src,
			  GFile *dest_dir,
			  const char *dest_fs_type,
			  int count)
{
	const char *editname;
	char *basename, *new_name;
	GFileInfo *info;
	GFile *dest;
	int max_length;

	max_length = get_max_name_length (dest_dir);

	dest = NULL;
	info = g_file_query_info (src,
				  G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME,
				  0, NULL, NULL);
	if (info != NULL) {
		editname = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
		
		if (editname != NULL) {
			new_name = get_link_name (editname, count, max_length);
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
			g_free (new_name);
		}
		
		g_object_unref (info);
	}

	if (dest == NULL) {
		basename = g_file_get_basename (src);
		make_file_name_valid_for_dest_fs (basename, dest_fs_type);

		if (g_utf8_validate (basename, -1, NULL)) {
			new_name = get_link_name (basename, count, max_length);
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child_for_display_name (dest_dir, new_name, NULL);
			g_free (new_name);
		} 

		if (dest == NULL) {
			if (count == 1) {
				new_name = g_strdup_printf ("%s.lnk", basename);
			} else {
				new_name = g_strdup_printf ("%s.lnk%d", basename, count);
			}
			make_file_name_valid_for_dest_fs (new_name, dest_fs_type);
			dest = g_file_get_child (dest_dir, new_name);
			g_free (new_name);
		}
		
		g_free (basename);
	}

	return dest;
}

static GFile *
get_target_file_with_custom_name (GFile *src,
				  GFile *dest_dir,
				  const char *dest_fs_type,
				  gboolean same_fs,
				  const gchar *custom_name)
{
	char *basename;
	GFile *dest;
	GFileInfo *info;
	char *copyname;

	dest = NULL;

	if (custom_name != NULL) {
		copyname = g_strdup (custom_name);
		make_file_name_valid_for_dest_fs (copyname, dest_fs_type);
		dest = g_file_get_child_for_display_name (dest_dir, copyname, NULL);

		g_free (copyname);
	}

	if (dest == NULL && !same_fs) {
		info = g_file_query_info (src,
					  G_FILE_ATTRIBUTE_STANDARD_COPY_NAME,
					  0, NULL, NULL);
		
		if (info) {
			copyname = g_strdup (g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_COPY_NAME));

			if (copyname) {
				make_file_name_valid_for_dest_fs (copyname, dest_fs_type);
				dest = g_file_get_child_for_display_name (dest_dir, copyname, NULL);
				g_free (copyname);
			}
			
			g_object_unref (info);
		}
	}

	if (dest == NULL) {
		basename = g_file_get_basename (src);
		make_file_name_valid_for_dest_fs (basename, dest_fs_type);
		dest = g_file_get_child (dest_dir, basename);
		g_free (basename);
	}
	
	return dest;
}

static GFile *
get_target_file (GFile *src,
		 GFile *dest_dir,
		 const char *dest_fs_type,
		 gboolean same_fs)
{
	return get_target_file_with_custom_name (src, dest_dir, dest_fs_type, same_fs, NULL);
}

static gboolean
has_fs_id (GFile *file, const char *fs_id)
{
	const char *id;
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_ID_FILESYSTEM,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);

	if (info) {
		id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
		
		if (id && strcmp (id, fs_id) == 0) {
			res = TRUE;
		}
		
		g_object_unref (info);
	}
	
	return res;
}

static gboolean
is_dir (GFile *file)
{
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);
	if (info) {
		res = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
		g_object_unref (info);
	}
	
	return res;
}

static void copy_move_file (CopyMoveJob *job,
			    GFile *src,
			    GFile *dest_dir,
			    gboolean same_fs,
			    gboolean unique_names,
			    char **dest_fs_type,
			    SourceInfo *source_info,
			    TransferInfo *transfer_info,
			    GHashTable *debuting_files,
			    GdkPoint *point,
			    gboolean overwrite,
			    gboolean *skipped_file,
			    gboolean readonly_source_fs);

typedef enum {
	CREATE_DEST_DIR_RETRY,
	CREATE_DEST_DIR_FAILED,
	CREATE_DEST_DIR_SUCCESS
} CreateDestDirResult;

static CreateDestDirResult
create_dest_dir (CommonJob *job,
		 GFile *src,
		 GFile **dest,
		 gboolean same_fs,
		 char **dest_fs_type)
{
	GError *error;
	GFile *new_dest, *dest_dir;
	char *primary, *secondary, *details;
	int response;
	gboolean handled_invalid_filename;

	handled_invalid_filename = *dest_fs_type != NULL;

 retry:
	/* First create the directory, then copy stuff to it before
	   copying the attributes, because we need to be sure we can write to it */
	
	error = NULL;
	if (!g_file_make_directory (*dest, job->cancellable, &error)) {
		if (IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
			return CREATE_DEST_DIR_FAILED;
		} else if (IS_IO_ERROR (error, INVALID_FILENAME) &&
			   !handled_invalid_filename) {
			handled_invalid_filename = TRUE;

			g_assert (*dest_fs_type == NULL);

			dest_dir = g_file_get_parent (*dest);

			if (dest_dir != NULL) {
				*dest_fs_type = query_fs_type (dest_dir, job->cancellable);

				new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
				g_object_unref (dest_dir);

				if (!g_file_equal (*dest, new_dest)) {
					g_object_unref (*dest);
					*dest = new_dest;
					g_error_free (error);
					return CREATE_DEST_DIR_RETRY;
				} else {
					g_object_unref (new_dest);
				}
			}
		}

		primary = f (_("Error while copying."));
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be copied because you do not have "
					 "permissions to create it in the destination."), src);
		} else {
			secondary = f (_("There was an error creating the folder \"%B\"."), src);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					FALSE,
					GTK_STOCK_CANCEL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) {
			/* Skip: Do Nothing  */
		} else if (response == 2) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
		return CREATE_DEST_DIR_FAILED;
	}
	nemo_file_changes_queue_file_added (*dest);

	if (job->undo_info != NULL) {
		nemo_file_undo_info_ext_add_origin_target_pair (NEMO_FILE_UNDO_INFO_EXT (job->undo_info),
								    src, *dest);
	}

	return CREATE_DEST_DIR_SUCCESS;
}

/* a return value of FALSE means retry, i.e.
 * the destination has changed and the source
 * is expected to re-try the preceeding
 * g_file_move() or g_file_copy() call with
 * the new destination.
 */
static gboolean
copy_move_directory (CopyMoveJob *copy_job,
		     GFile *src,
		     GFile **dest,
		     gboolean same_fs,
		     gboolean create_dest,
		     char **parent_dest_fs_type,
		     SourceInfo *source_info,
		     TransferInfo *transfer_info,
		     GHashTable *debuting_files,
		     gboolean *skipped_file,
		     gboolean readonly_source_fs)
{
	GFileInfo *info;
	GError *error;
	GFile *src_file;
	GFileEnumerator *enumerator;
	char *primary, *secondary, *details;
	char *dest_fs_type;
	int response;
	gboolean skip_error;
	gboolean local_skipped_file;
	CommonJob *job;
	GFileCopyFlags flags;

	job = (CommonJob *)copy_job;
	
	if (create_dest) {
		switch (create_dest_dir (job, src, dest, same_fs, parent_dest_fs_type)) {
			case CREATE_DEST_DIR_RETRY:
				/* next time copy_move_directory() is called,
				 * create_dest will be FALSE if a directory already
				 * exists under the new name (i.e. WOULD_RECURSE)
				 */
				return FALSE;

			case CREATE_DEST_DIR_FAILED:
				*skipped_file = TRUE;
				return TRUE;

			case CREATE_DEST_DIR_SUCCESS:
			default:
				break;
		}

		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (*dest), GINT_TO_POINTER (TRUE));
		}

	}

	local_skipped_file = FALSE;
	dest_fs_type = NULL;
	
	skip_error = should_skip_readdir_error (job, src);
 retry:
	error = NULL;
	enumerator = g_file_enumerate_children (src,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;

		while (!job_aborted (job) &&
		       (info = g_file_enumerator_next_file (enumerator, job->cancellable, skip_error?NULL:&error)) != NULL) {
			src_file = g_file_get_child (src,
						     g_file_info_get_name (info));
			copy_move_file (copy_job, src_file, *dest, same_fs, FALSE, &dest_fs_type,
					source_info, transfer_info, NULL, NULL, FALSE, &local_skipped_file,
					readonly_source_fs);
			g_object_unref (src_file);
			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		g_object_unref (enumerator);
		
		if (error && IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
		} else if (error) {
			if (copy_job->is_move) {
				primary = f (_("Error while moving."));
			} else {
				primary = f (_("Error while copying."));
			}
			details = NULL;
			
			if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
				secondary = f (_("Files in the folder \"%B\" cannot be copied because you do "
						 "not have permissions to see them."), src);
			} else {
				secondary = f (_("There was an error getting information about the files in the folder \"%B\"."), src);
				details = error->message;
			}
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						FALSE,
						GTK_STOCK_CANCEL, _("_Skip files"),
						NULL);
			
			g_error_free (error);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 1) {
				/* Skip: Do Nothing */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
		}

		/* Count the copied directory as a file */
		transfer_info->num_files ++;
		report_copy_progress (copy_job, source_info, transfer_info);

		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (*dest), GINT_TO_POINTER (create_dest));
		}
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	} else {
		if (copy_job->is_move) {
			primary = f (_("Error while moving."));
		} else {
			primary = f (_("Error while copying."));
		}
		details = NULL;
		
		if (IS_IO_ERROR (error, PERMISSION_DENIED)) {
			secondary = f (_("The folder \"%B\" cannot be copied because you do not have "
					 "permissions to read it."), src);
		} else {
			secondary = f (_("There was an error reading the folder \"%B\"."), src);
			details = error->message;
		}
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					FALSE,
					GTK_STOCK_CANCEL, SKIP, RETRY,
					NULL);

		g_error_free (error);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) {
			/* Skip: Do Nothing  */
			local_skipped_file = TRUE;
		} else if (response == 2) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}

	if (create_dest) {
		flags = (readonly_source_fs) ? G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_TARGET_DEFAULT_PERMS 
					     : G_FILE_COPY_NOFOLLOW_SYMLINKS;
		/* Ignore errors here. Failure to copy metadata is not a hard error */
		g_file_copy_attributes (src, *dest,
					flags,
					job->cancellable, NULL);
	}

	if (!job_aborted (job) && copy_job->is_move &&
	    /* Don't delete source if there was a skipped file */
	    !local_skipped_file) {
		if (!g_file_delete (src, job->cancellable, &error)) {
			if (job->skip_all_error) {
				goto skip;
			}
			primary = f (_("Error while moving \"%B\"."), src);
			secondary = f (_("Could not remove the source folder."));
			details = error->message;
			
			response = run_warning (job,
						primary,
						secondary,
						details,
						(source_info->num_files - transfer_info->num_files) > 1,
						GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
						NULL);
			
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (job);
			} else if (response == 1) { /* skip all */
				job->skip_all_error = TRUE;
				local_skipped_file = TRUE;
			} else if (response == 2) { /* skip */
				local_skipped_file = TRUE;
			} else {
				g_assert_not_reached ();
			}
			
		skip:
			g_error_free (error);
		}
	}

	if (local_skipped_file) {
		*skipped_file = TRUE;
	}

	g_free (dest_fs_type);
	return TRUE;
}

static gboolean
remove_target_recursively (CommonJob *job,
			   GFile *src,
			   GFile *toplevel_dest,
			   GFile *file)
{
	GFileEnumerator *enumerator;
	GError *error;
	GFile *child;
	gboolean stop;
	char *primary, *secondary, *details;
	int response;
	GFileInfo *info;

	stop = FALSE;
	
	error = NULL;
	enumerator = g_file_enumerate_children (file,
						G_FILE_ATTRIBUTE_STANDARD_NAME,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						job->cancellable,
						&error);
	if (enumerator) {
		error = NULL;
		
		while (!job_aborted (job) &&
		       (info = g_file_enumerator_next_file (enumerator, job->cancellable, &error)) != NULL) {
			child = g_file_get_child (file,
						  g_file_info_get_name (info));
			if (!remove_target_recursively (job, src, toplevel_dest, child)) {
				stop = TRUE;
				break;
			}
			g_object_unref (child);
			g_object_unref (info);
		}
		g_file_enumerator_close (enumerator, job->cancellable, NULL);
		g_object_unref (enumerator);
		
	} else if (IS_IO_ERROR (error, NOT_DIRECTORY)) {
		/* Not a dir, continue */
		g_error_free (error);
		
	} else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	} else {
		if (job->skip_all_error) {
			goto skip1;
		}
		
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("Could not remove files from the already existing folder %F."), file);
		details = error->message;

		/* set show_all to TRUE here, as we don't know how many
		 * files we'll end up processing yet.
		 */
		response = run_warning (job,
					primary,
					secondary,
					details,
					TRUE,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	skip1:
		g_error_free (error);
		
		stop = TRUE;
	}

	if (stop) {
		return FALSE;
	}

	error = NULL;
	
	if (!g_file_delete (file, job->cancellable, &error)) {
		if (job->skip_all_error ||
		    IS_IO_ERROR (error, CANCELLED)) {
			goto skip2;
		}
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("Could not remove the already existing file %F."), file);
		details = error->message;

		/* set show_all to TRUE here, as we don't know how many
		 * files we'll end up processing yet.
		 */
		response = run_warning (job,
					primary,
					secondary,
					details,
					TRUE,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}

	skip2:
		g_error_free (error);
		
		return FALSE;
	}
	nemo_file_changes_queue_file_removed (file);
	
	return TRUE;
	
}

typedef struct {
	CopyMoveJob *job;
	goffset last_size;
	SourceInfo *source_info;
	TransferInfo *transfer_info;
} ProgressData;

static void
copy_file_progress_callback (goffset current_num_bytes,
			     goffset total_num_bytes,
			     gpointer user_data)
{
	ProgressData *pdata;
	goffset new_size;

	pdata = user_data;
	
	new_size = current_num_bytes - pdata->last_size;

	if (new_size > 0) {
		pdata->transfer_info->num_bytes += new_size;
		pdata->last_size = current_num_bytes;
		report_copy_progress (pdata->job,
				      pdata->source_info,
				      pdata->transfer_info);
	}
}

static gboolean
test_dir_is_parent (GFile *child, GFile *root)
{
	GFile *f;
	
	f = g_file_dup (child);
	while (f) {
		if (g_file_equal (f, root)) {
			g_object_unref (f);
			return TRUE;
		}
		f = g_file_get_parent (f);
	}
	if (f) {
		g_object_unref (f);
	}
	return FALSE;
}

static char *
query_fs_type (GFile *file,
	       GCancellable *cancellable)
{
	GFileInfo *fsinfo;
	char *ret;

	ret = NULL;

	fsinfo = g_file_query_filesystem_info (file,
					       G_FILE_ATTRIBUTE_FILESYSTEM_TYPE,
					       cancellable,
					       NULL);
	if (fsinfo != NULL) {
		ret = g_strdup (g_file_info_get_attribute_string (fsinfo, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE));
		g_object_unref (fsinfo);
	}

	if (ret == NULL) {
		/* ensure that we don't attempt to query
		 * the FS type for each file in a given
		 * directory, if it can't be queried. */
		ret = g_strdup ("");
	}

	return ret;
}

static gboolean
is_trusted_desktop_file (GFile *file,
			 GCancellable *cancellable)
{
	char *basename;
	gboolean res;
	GFileInfo *info;

	/* Don't trust non-local files */
	if (!g_file_is_native (file)) {
		return FALSE;
	}
	
	basename = g_file_get_basename (file);
	if (!g_str_has_suffix (basename, ".desktop")) {
		g_free (basename);
		return FALSE;
	}
	g_free (basename);

	info = g_file_query_info (file, 
				  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				  G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  cancellable,
				  NULL);

	if (info == NULL) {
		return FALSE;
	}

	res = FALSE;
	
	/* Weird file => not trusted,
	   Already executable => no need to mark trusted */
	if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR &&
	    !g_file_info_get_attribute_boolean (info,
						G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE) &&
	    nemo_is_in_system_dir (file)) {
		res = TRUE;
	}
	g_object_unref (info);
	
	return res;
}

typedef struct {
	int id;
	char *new_name;
	gboolean apply_to_all;
} ConflictResponseData;

typedef struct {
	GFile *src;
	GFile *dest;
	GFile *dest_dir;
	GtkWindow *parent;
	ConflictResponseData *resp_data;
} ConflictDialogData;

static gboolean
do_run_conflict_dialog (gpointer _data)
{
	ConflictDialogData *data = _data;
	GtkWidget *dialog;
	int response;

	dialog = nemo_file_conflict_dialog_new (data->parent,
						    data->src,
						    data->dest,
						    data->dest_dir);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == CONFLICT_RESPONSE_RENAME) {
		data->resp_data->new_name = 
			nemo_file_conflict_dialog_get_new_name (NEMO_FILE_CONFLICT_DIALOG (dialog));
	} else if (response != GTK_RESPONSE_CANCEL ||
		   response != GTK_RESPONSE_NONE) {
		   data->resp_data->apply_to_all =
			   nemo_file_conflict_dialog_get_apply_to_all 
				(NEMO_FILE_CONFLICT_DIALOG (dialog));
	}

	data->resp_data->id = response;

	gtk_widget_destroy (dialog);

	return FALSE;
}

static ConflictResponseData *
run_conflict_dialog (CommonJob *job,
		     GFile *src,
		     GFile *dest,
		     GFile *dest_dir)
{
	ConflictDialogData *data;
	ConflictResponseData *resp_data;

	g_timer_stop (job->time);

	data = g_slice_new0 (ConflictDialogData);
	data->parent = job->parent_window;
	data->src = src;
	data->dest = dest;
	data->dest_dir = dest_dir;

	resp_data = g_slice_new0 (ConflictResponseData);
	resp_data->new_name = NULL;
	data->resp_data = resp_data;

	nemo_progress_info_pause (job->progress);
	g_io_scheduler_job_send_to_mainloop (job->io_job,
					     do_run_conflict_dialog,
					     data,
					     NULL);
	nemo_progress_info_resume (job->progress);

	g_slice_free (ConflictDialogData, data);

	g_timer_continue (job->time);

	return resp_data;
}

static void
conflict_response_data_free (ConflictResponseData *data)
{
	g_free (data->new_name);
	g_slice_free (ConflictResponseData, data);
}

static GFile *
get_target_file_for_display_name (GFile *dir,
				  const gchar *name)
{
	GFile *dest;

	dest = NULL;
	dest = g_file_get_child_for_display_name (dir, name, NULL);

	if (dest == NULL) {
		dest = g_file_get_child (dir, name);
	}
	
	return dest;		
}

/* Debuting files is non-NULL only for toplevel items */
static void
copy_move_file (CopyMoveJob *copy_job,
		GFile *src,
		GFile *dest_dir,
		gboolean same_fs,
		gboolean unique_names,
		char **dest_fs_type,
		SourceInfo *source_info,
		TransferInfo *transfer_info,
		GHashTable *debuting_files,
		GdkPoint *position,
		gboolean overwrite,
		gboolean *skipped_file,
		gboolean readonly_source_fs)
{
	GFile *dest, *new_dest;
	GError *error;
	GFileCopyFlags flags;
	char *primary, *secondary, *details;
	int response;
	ProgressData pdata;
	gboolean would_recurse, is_merge;
	CommonJob *job;
	gboolean res;
	int unique_name_nr;
	gboolean handled_invalid_filename;

	job = (CommonJob *)copy_job;
	
	if (should_skip_file (job, src)) {
		*skipped_file = TRUE;
		return;
	}

	unique_name_nr = 1;

	/* another file in the same directory might have handled the invalid
	 * filename condition for us
	 */
	handled_invalid_filename = *dest_fs_type != NULL;

	if (unique_names) {
		dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr++);
	} else if (copy_job->target_name != NULL) {
		dest = get_target_file_with_custom_name (src, dest_dir, *dest_fs_type, same_fs,
							 copy_job->target_name);
	} else {
		dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
	}

	/* Don't allow recursive move/copy into itself.  
	 * (We would get a file system error if we proceeded but it is nicer to 
	 * detect and report it at this level) */
	if (test_dir_is_parent (dest_dir, src)) {
		if (job->skip_all_error) {
			goto out;
		}
		
		/*  the run_warning() frees all strings passed in automatically  */
		primary = copy_job->is_move ? g_strdup (_("You cannot move a folder into itself."))
					    : g_strdup (_("You cannot copy a folder into itself."));
		secondary = g_strdup (_("The destination folder is inside the source folder."));
		
		response = run_warning (job,
					primary,
					secondary,
					NULL,
					(source_info->num_files - transfer_info->num_files) > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}

		goto out;
	}

	/* Don't allow copying over the source or one of the parents of the source.
	 */
	if (test_dir_is_parent (src, dest)) {
		if (job->skip_all_error) {
			goto out;
		}
		
		/*  the run_warning() frees all strings passed in automatically  */
		primary = copy_job->is_move ? g_strdup (_("You cannot move a file over itself."))
					    : g_strdup (_("You cannot copy a file over itself."));
		secondary = g_strdup (_("The source file would be overwritten by the destination."));
		
		response = run_warning (job,
					primary,
					secondary,
					NULL,
					(source_info->num_files - transfer_info->num_files) > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}

		goto out;
	}

	
 retry:
	
	error = NULL;
	flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
	if (overwrite) {
		flags |= G_FILE_COPY_OVERWRITE;
	}
	if (readonly_source_fs) {
		flags |= G_FILE_COPY_TARGET_DEFAULT_PERMS;
	}

	pdata.job = copy_job;
	pdata.last_size = 0;
	pdata.source_info = source_info;
	pdata.transfer_info = transfer_info;

	if (copy_job->is_move) {
		res = g_file_move (src, dest,
				   flags,
				   job->cancellable,
				   copy_file_progress_callback,
				   &pdata,
				   &error);
	} else {
		res = g_file_copy (src, dest,
				   flags,
				   job->cancellable,
				   copy_file_progress_callback,
				   &pdata,
				   &error);
	}
	
	if (res) {
		transfer_info->num_files ++;
		report_copy_progress (copy_job, source_info, transfer_info);

		if (debuting_files) {
			if (position) {
				nemo_file_changes_queue_schedule_position_set (dest, *position, job->screen_num);
			} else {
				nemo_file_changes_queue_schedule_position_remove (dest);
			}
			
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}
		if (copy_job->is_move) {
			nemo_file_changes_queue_file_moved (src, dest);
		} else {
			nemo_file_changes_queue_file_added (dest);
		}

		/* If copying a trusted desktop file to the desktop,
		   mark it as trusted. */
		if (copy_job->desktop_location != NULL &&
		    g_file_equal (copy_job->desktop_location, dest_dir) &&
		    is_trusted_desktop_file (src, job->cancellable)) {
			mark_desktop_file_trusted (job,
						   job->cancellable,
						   dest,
						   FALSE);
		}

		if (job->undo_info != NULL) {
			nemo_file_undo_info_ext_add_origin_target_pair (NEMO_FILE_UNDO_INFO_EXT (job->undo_info),
									    src, dest);
		}

		g_object_unref (dest);
		return;
	}

	if (!handled_invalid_filename &&
	    IS_IO_ERROR (error, INVALID_FILENAME)) {
		handled_invalid_filename = TRUE;

		g_assert (*dest_fs_type == NULL);
		*dest_fs_type = query_fs_type (dest_dir, job->cancellable);

		if (unique_names) {
			new_dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr);
		} else {
			new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
		}

		if (!g_file_equal (dest, new_dest)) {
			g_object_unref (dest);
			dest = new_dest;

			g_error_free (error);
			goto retry;
		} else {
			g_object_unref (new_dest);
		}
	}

	/* Conflict */
	if (!overwrite &&
	    IS_IO_ERROR (error, EXISTS)) {
		gboolean is_merge;
		ConflictResponseData *response;

		g_error_free (error);

		if (unique_names) {
			g_object_unref (dest);
			dest = get_unique_target_file (src, dest_dir, same_fs, *dest_fs_type, unique_name_nr++);
			goto retry;
		}

		is_merge = FALSE;

		if (is_dir (dest) && is_dir (src)) {
			is_merge = TRUE;
		}

		if ((is_merge && job->merge_all) ||
		    (!is_merge && job->replace_all)) {
			overwrite = TRUE;
			goto retry;
		}

		if (job->skip_all_conflict) {
			goto out;
		}

		response = run_conflict_dialog (job, src, dest, dest_dir);	

		if (response->id == GTK_RESPONSE_CANCEL ||
		    response->id == GTK_RESPONSE_DELETE_EVENT) {
			conflict_response_data_free (response);
			abort_job (job);
		} else if (response->id == CONFLICT_RESPONSE_SKIP) {
			if (response->apply_to_all) {
				job->skip_all_conflict = TRUE;
			}
			conflict_response_data_free (response);
		} else if (response->id == CONFLICT_RESPONSE_REPLACE) { /* merge/replace */
			if (response->apply_to_all) {
				if (is_merge) {
					job->merge_all = TRUE;
				} else {
					job->replace_all = TRUE;
				}
			}
			overwrite = TRUE;
			conflict_response_data_free (response);
			goto retry;
		} else if (response->id == CONFLICT_RESPONSE_RENAME) {
			g_object_unref (dest);
			dest = get_target_file_for_display_name (dest_dir,
								 response->new_name);
			conflict_response_data_free (response);
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}
	
	else if (overwrite &&
		 IS_IO_ERROR (error, IS_DIRECTORY)) {

		g_error_free (error);
		
		if (remove_target_recursively (job, src, dest, dest)) {
			goto retry;
		}
	}
	
	/* Needs to recurse */
	else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
		 IS_IO_ERROR (error, WOULD_MERGE)) {
		is_merge = error->code == G_IO_ERROR_WOULD_MERGE;
		would_recurse = error->code == G_IO_ERROR_WOULD_RECURSE;
		g_error_free (error);

		if (overwrite && would_recurse) {
			error = NULL;
			
			/* Copying a dir onto file, first remove the file */
			if (!g_file_delete (dest, job->cancellable, &error) &&
			    !IS_IO_ERROR (error, NOT_FOUND)) {
				if (job->skip_all_error) {
					g_error_free (error);
					goto out;
				}
				if (copy_job->is_move) {
					primary = f (_("Error while moving \"%B\"."), src);
				} else {
					primary = f (_("Error while copying \"%B\"."), src);
				}
				secondary = f (_("Could not remove the already existing file with the same name in %F."), dest_dir);
				details = error->message;

				/* setting TRUE on show_all here, as we could have
				 * another error on the same file later.
				 */
				response = run_warning (job,
							primary,
							secondary,
							details,
							TRUE,
							GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
							NULL);
				
				g_error_free (error);
				
				if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
					abort_job (job);
				} else if (response == 1) { /* skip all */
					job->skip_all_error = TRUE;
				} else if (response == 2) { /* skip */
					/* do nothing */
				} else {
					g_assert_not_reached ();
				}
				goto out;
				
			}
			if (error) {
				g_error_free (error);
				error = NULL;
			}
			nemo_file_changes_queue_file_removed (dest);
		}

		if (is_merge) {
			/* On merge we now write in the target directory, which may not
			   be in the same directory as the source, even if the parent is
			   (if the merged directory is a mountpoint). This could cause
			   problems as we then don't transcode filenames.
			   We just set same_fs to FALSE which is safe but a bit slower. */
			same_fs = FALSE;
		}
		
		if (!copy_move_directory (copy_job, src, &dest, same_fs,
					  would_recurse, dest_fs_type,
					  source_info, transfer_info,
					  debuting_files, skipped_file,
					  readonly_source_fs)) {
			/* destination changed, since it was an invalid file name */
			g_assert (*dest_fs_type != NULL);
			handled_invalid_filename = TRUE;
			goto retry;
		}

		g_object_unref (dest);
		return;
	}
	
	else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	}
	
	/* Other error */
	else {
		if (job->skip_all_error) {
			g_error_free (error);
			goto out;
		}
		primary = f (_("Error while copying \"%B\"."), src);
		secondary = f (_("There was an error copying the file into %F."), dest_dir);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					(source_info->num_files - transfer_info->num_files) > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	}
 out:
	*skipped_file = TRUE; /* Or aborted, but same-same */
	g_object_unref (dest);
}

static void
copy_files (CopyMoveJob *job,
	    const char *dest_fs_id,
	    SourceInfo *source_info,
	    TransferInfo *transfer_info)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	gboolean skipped_file;
	gboolean unique_names;
	GFile *dest;
	GFile *source_dir;
	char *dest_fs_type;
	GFileInfo *inf;
	gboolean readonly_source_fs;

	dest_fs_type = NULL;
	readonly_source_fs = FALSE;

	common = &job->common;

	report_copy_progress (job, source_info, transfer_info);

	/* Query the source dir, not the file because if its a symlink we'll follow it */
	source_dir = g_file_get_parent ((GFile *) job->files->data);
	if (source_dir) {
		inf = g_file_query_filesystem_info (source_dir, "filesystem::readonly", NULL, NULL);
		if (inf != NULL) {
			readonly_source_fs = g_file_info_get_attribute_boolean (inf, "filesystem::readonly");
			g_object_unref (inf);
		}
		g_object_unref (source_dir);
	}

	unique_names = (job->destination == NULL);
	i = 0;
	for (l = job->files;
	     l != NULL && !job_aborted (common);
	     l = l->next) {
		src = l->data;

		if (i < job->n_icon_positions) {
			point = &job->icon_positions[i];
		} else {
			point = NULL;
		}

		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}

		if (job->destination) {
			dest = g_object_ref (job->destination);
		} else {
			dest = g_file_get_parent (src);
			
		}
		if (dest) {
			skipped_file = FALSE;
			copy_move_file (job, src, dest,
					same_fs, unique_names,
					&dest_fs_type,
					source_info, transfer_info,
					job->debuting_files,
					point, FALSE, &skipped_file,
					readonly_source_fs);
			g_object_unref (dest);
		}
		i++;
	}

	g_free (dest_fs_type);
}

static gboolean
copy_job_done (gpointer user_data)
{
	CopyMoveJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->debuting_files, 
				    !job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}

	g_list_free_full (job->files, g_object_unref);
	if (job->destination) {
		g_object_unref (job->destination);
	}
	if (job->desktop_location) {
		g_object_unref (job->desktop_location);
	}
	g_hash_table_unref (job->debuting_files);
	g_free (job->icon_positions);
	g_free (job->target_name);

	g_clear_object (&job->fake_display_source);
	
	finalize_common ((CommonJob *)job);

	nemo_file_changes_consume_changes (TRUE);
	return FALSE;
}

static gboolean
copy_job (GIOSchedulerJob *io_job,
	  GCancellable *cancellable,
	  gpointer user_data)
{
	CopyMoveJob *job;
	CommonJob *common;
	SourceInfo source_info;
	TransferInfo transfer_info;
	char *dest_fs_id;
	GFile *dest;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	dest_fs_id = NULL;
	
	nemo_progress_info_start (job->common.progress);
	
	scan_sources (job->files,
		      &source_info,
		      common,
		      OP_KIND_COPY);
	if (job_aborted (common)) {
		goto aborted;
	}

	if (job->destination) {
		dest = g_object_ref (job->destination);
	} else {
		/* Duplication, no dest,
		 * use source for free size, etc
		 */
		dest = g_file_get_parent (job->files->data);
	}
	
	verify_destination (&job->common,
			    dest,
			    &dest_fs_id,
			    source_info.num_bytes);
	g_object_unref (dest);
	if (job_aborted (common)) {
		goto aborted;
	}

	g_timer_start (job->common.time);
	
	memset (&transfer_info, 0, sizeof (transfer_info));
	copy_files (job,
		    dest_fs_id,
		    &source_info, &transfer_info);

 aborted:
	
	g_free (dest_fs_id);
	
	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   copy_job_done,
						   job,
						   NULL);

	return FALSE;
}

void
nemo_file_operations_copy_file (GFile *source_file,
				    GFile *target_dir,
				    const gchar *source_display_name,
				    const gchar *new_name,
				    GtkWindow *parent_window,
				    NemoCopyCallback done_callback,
				    gpointer done_callback_data)
{
	CopyMoveJob *job;

	job = op_job_new (CopyMoveJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = g_list_append (NULL, g_object_ref (source_file));
	job->destination = g_object_ref (target_dir);
	job->target_name = g_strdup (new_name);
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	if (source_display_name != NULL) {
		gchar *path;

		path = g_build_filename ("/", source_display_name, NULL);
		job->fake_display_source = g_file_new_for_path (path);

		g_free (path);
	}

	inhibit_power_manager ((CommonJob *)job, _("Copying Files"));

	g_io_scheduler_push_job (copy_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

void
nemo_file_operations_copy (GList *files,
			       GArray *relative_item_points,
			       GFile *target_dir,
			       GtkWindow *parent_window,
			       NemoCopyCallback  done_callback,
			       gpointer done_callback_data)
{
	CopyMoveJob *job;

	job = op_job_new (CopyMoveJob, parent_window);
	job->desktop_location = nemo_get_desktop_location ();
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = g_object_ref (target_dir);
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	inhibit_power_manager ((CommonJob *)job, _("Copying Files"));

	if (!nemo_file_undo_manager_pop_flag ()) {
		GFile* src_dir;

		src_dir = g_file_get_parent (files->data);
		job->common.undo_info = nemo_file_undo_info_ext_new (NEMO_FILE_UNDO_OP_COPY,
									 g_list_length (files),
									 src_dir, target_dir);

		g_object_unref (src_dir);
	}

	g_io_scheduler_push_job (copy_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

static void
report_move_progress (CopyMoveJob *move_job, int total, int left)
{
	CommonJob *job;

	job = (CommonJob *)move_job;
	
	nemo_progress_info_take_status (job->progress,
					    f (_("Preparing to Move to \"%B\""),
					       move_job->destination));

	nemo_progress_info_take_details (job->progress,
					     f (ngettext ("Preparing to move %'d file",
							  "Preparing to move %'d files",
							  left), left));

	nemo_progress_info_pulse_progress (job->progress);
}

typedef struct {
	GFile *file;
	gboolean overwrite;
	gboolean has_position;
	GdkPoint position;
} MoveFileCopyFallback;

static MoveFileCopyFallback *
move_copy_file_callback_new (GFile *file,
			     gboolean overwrite,
			     GdkPoint *position)
{
	MoveFileCopyFallback *fallback;

	fallback = g_new (MoveFileCopyFallback, 1);
	fallback->file = file;
	fallback->overwrite = overwrite;
	if (position) {
		fallback->has_position = TRUE;
		fallback->position = *position;
	} else {
		fallback->has_position = FALSE;
	}

	return fallback;
}

static GList *
get_files_from_fallbacks (GList *fallbacks)
{
	MoveFileCopyFallback *fallback;
	GList *res, *l;

	res = NULL;
	for (l = fallbacks; l != NULL; l = l->next) {
		fallback = l->data;
		res = g_list_prepend (res, fallback->file);
	}
	return g_list_reverse (res);
}

static void
move_file_prepare (CopyMoveJob *move_job,
		   GFile *src,
		   GFile *dest_dir,
		   gboolean same_fs,
		   char **dest_fs_type,
		   GHashTable *debuting_files,
		   GdkPoint *position,
		   GList **fallback_files,
		   int files_left)
{
	GFile *dest, *new_dest;
	GError *error;
	CommonJob *job;
	gboolean overwrite;
	char *primary, *secondary, *details;
	int response;
	GFileCopyFlags flags;
	MoveFileCopyFallback *fallback;
	gboolean handled_invalid_filename;

	overwrite = FALSE;
	handled_invalid_filename = *dest_fs_type != NULL;

	job = (CommonJob *)move_job;
	
	dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);


	/* Don't allow recursive move/copy into itself.  
	 * (We would get a file system error if we proceeded but it is nicer to 
	 * detect and report it at this level) */
	if (test_dir_is_parent (dest_dir, src)) {
		if (job->skip_all_error) {
			goto out;
		}
		
		/*  the run_warning() frees all strings passed in automatically  */
		primary = move_job->is_move ? g_strdup (_("You cannot move a folder into itself."))
					    : g_strdup (_("You cannot copy a folder into itself."));
		secondary = g_strdup (_("The destination folder is inside the source folder."));
		
		response = run_warning (job,
					primary,
					secondary,
					NULL,
					files_left > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}

		goto out;
	}

 retry:
	
	flags = G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_NO_FALLBACK_FOR_MOVE;
	if (overwrite) {
		flags |= G_FILE_COPY_OVERWRITE;
	}
	
	error = NULL;
	if (g_file_move (src, dest,
			 flags,
			 job->cancellable,
			 NULL,
			 NULL,
			 &error)) {
		
		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}

		nemo_file_changes_queue_file_moved (src, dest);

		if (position) {
			nemo_file_changes_queue_schedule_position_set (dest, *position, job->screen_num);
		} else {
			nemo_file_changes_queue_schedule_position_remove (dest);
		}

		if (job->undo_info != NULL) {
			nemo_file_undo_info_ext_add_origin_target_pair (NEMO_FILE_UNDO_INFO_EXT (job->undo_info),
									    src, dest);
		}

		return;
	}

	if (IS_IO_ERROR (error, INVALID_FILENAME) &&
	    !handled_invalid_filename) {
		handled_invalid_filename = TRUE;

		g_assert (*dest_fs_type == NULL);
		*dest_fs_type = query_fs_type (dest_dir, job->cancellable);

		new_dest = get_target_file (src, dest_dir, *dest_fs_type, same_fs);
		if (!g_file_equal (dest, new_dest)) {
			g_object_unref (dest);
			dest = new_dest;
			goto retry;
		} else {
			g_object_unref (new_dest);
		}
	}

	/* Conflict */
	else if (!overwrite &&
		 IS_IO_ERROR (error, EXISTS)) {
		gboolean is_merge;
		ConflictResponseData *response;
		
		g_error_free (error);

		is_merge = FALSE;
		if (is_dir (dest) && is_dir (src)) {
			is_merge = TRUE;
		}

		if ((is_merge && job->merge_all) ||
		    (!is_merge && job->replace_all)) {
			overwrite = TRUE;
			goto retry;
		}

		if (job->skip_all_conflict) {
			goto out;
		}

		response = run_conflict_dialog (job, src, dest, dest_dir);

		if (response->id == GTK_RESPONSE_CANCEL ||
		    response->id == GTK_RESPONSE_DELETE_EVENT) {
			conflict_response_data_free (response);	
			abort_job (job);
		} else if (response->id == CONFLICT_RESPONSE_SKIP) {
			if (response->apply_to_all) {
				job->skip_all_conflict = TRUE;
			}
			conflict_response_data_free (response);
		} else if (response->id == CONFLICT_RESPONSE_REPLACE) { /* merge/replace */
			if (response->apply_to_all) {
				if (is_merge) {
					job->merge_all = TRUE;
				} else {
					job->replace_all = TRUE;
				}
			}
			overwrite = TRUE;
			conflict_response_data_free (response);
			goto retry;
		} else if (response->id == CONFLICT_RESPONSE_RENAME) {
			g_object_unref (dest);
			dest = get_target_file_for_display_name (dest_dir,
								 response->new_name);
			conflict_response_data_free (response);
			goto retry;
		} else {
			g_assert_not_reached ();
		}
	}

	else if (IS_IO_ERROR (error, WOULD_RECURSE) ||
		 IS_IO_ERROR (error, WOULD_MERGE) ||
		 IS_IO_ERROR (error, NOT_SUPPORTED) ||
		 (overwrite && IS_IO_ERROR (error, IS_DIRECTORY))) {
		g_error_free (error);
		
		fallback = move_copy_file_callback_new (src,
							overwrite, 
							position);
		*fallback_files = g_list_prepend (*fallback_files, fallback);
	}
	
	else if (IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	}
	
	/* Other error */
	else {
		if (job->skip_all_error) {
			goto out;
		}
		primary = f (_("Error while moving \"%B\"."), src);
		secondary = f (_("There was an error moving the file into %F."), dest_dir);
		details = error->message;
		
		response = run_warning (job,
					primary,
					secondary,
					details,
					files_left > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		g_error_free (error);
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (job);
		} else if (response == 1) { /* skip all */
			job->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	}
	
 out:
	g_object_unref (dest);
}

static void
move_files_prepare (CopyMoveJob *job,
		    const char *dest_fs_id,
		    char **dest_fs_type,
		    GList **fallbacks)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	int total, left;

	common = &job->common;

	total = left = g_list_length (job->files);

	report_move_progress (job, total, left);

	i = 0;
	for (l = job->files;
	     l != NULL && !job_aborted (common);
	     l = l->next) {
		src = l->data;

		if (i < job->n_icon_positions) {
			point = &job->icon_positions[i];
		} else {
			point = NULL;
		}

		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}
		
		move_file_prepare (job, src, job->destination,
				   same_fs, dest_fs_type,
				   job->debuting_files,
				   point,
				   fallbacks,
				   left);
		report_move_progress (job, total, --left);
		i++;
	}

	*fallbacks = g_list_reverse (*fallbacks);

	
}

static void
move_files (CopyMoveJob *job,
	    GList *fallbacks,
	    const char *dest_fs_id,
	    char **dest_fs_type,
	    SourceInfo *source_info,
	    TransferInfo *transfer_info)
{
	CommonJob *common;
	GList *l;
	GFile *src;
	gboolean same_fs;
	int i;
	GdkPoint *point;
	gboolean skipped_file;
	MoveFileCopyFallback *fallback;
common = &job->common;

	report_copy_progress (job, source_info, transfer_info);
	
	i = 0;
	for (l = fallbacks;
	     l != NULL && !job_aborted (common);
	     l = l->next) {
		fallback = l->data;
		src = fallback->file;

		if (fallback->has_position) {
			point = &fallback->position;
		} else {
			point = NULL;
		}
		
		same_fs = FALSE;
		if (dest_fs_id) {
			same_fs = has_fs_id (src, dest_fs_id);
		}

		/* Set overwrite to true, as the user has
		   selected overwrite on all toplevel items */
		skipped_file = FALSE;
		copy_move_file (job, src, job->destination,
				same_fs, FALSE, dest_fs_type,
				source_info, transfer_info,
				job->debuting_files,
				point, fallback->overwrite, &skipped_file, FALSE);
		i++;
	}
}


static gboolean
move_job_done (gpointer user_data)
{
	CopyMoveJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->debuting_files,
				    !job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}

	g_list_free_full (job->files, g_object_unref);
	g_object_unref (job->destination);
	g_hash_table_unref (job->debuting_files);
	g_free (job->icon_positions);
	
	finalize_common ((CommonJob *)job);

	nemo_file_changes_consume_changes (TRUE);
	return FALSE;
}

static gboolean
move_job (GIOSchedulerJob *io_job,
	  GCancellable *cancellable,
	  gpointer user_data)
{
	CopyMoveJob *job;
	CommonJob *common;
	GList *fallbacks;
	SourceInfo source_info;
	TransferInfo transfer_info;
	char *dest_fs_id;
	char *dest_fs_type;
	GList *fallback_files;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	dest_fs_id = NULL;
	dest_fs_type = NULL;

	fallbacks = NULL;
	
	nemo_progress_info_start (job->common.progress);
	
	verify_destination (&job->common,
			    job->destination,
			    &dest_fs_id,
			    -1);
	if (job_aborted (common)) {
		goto aborted;
	}

	/* This moves all files that we can do without copy + delete */
	move_files_prepare (job, dest_fs_id, &dest_fs_type, &fallbacks);
	if (job_aborted (common)) {
		goto aborted;
	}

	/* The rest we need to do deep copy + delete behind on,
	   so scan for size */

	fallback_files = get_files_from_fallbacks (fallbacks);
	scan_sources (fallback_files,
		      &source_info,
		      common,
		      OP_KIND_MOVE);
	
	g_list_free (fallback_files);
	
	if (job_aborted (common)) {
		goto aborted;
	}

	verify_destination (&job->common,
			    job->destination,
			    NULL,
			    source_info.num_bytes);
	if (job_aborted (common)) {
		goto aborted;
	}

	memset (&transfer_info, 0, sizeof (transfer_info));
	move_files (job,
		    fallbacks,
		    dest_fs_id, &dest_fs_type,
		    &source_info, &transfer_info);

 aborted:
	g_list_free_full (fallbacks, g_free);

	g_free (dest_fs_id);
	g_free (dest_fs_type);
	
	g_io_scheduler_job_send_to_mainloop (io_job,
					     move_job_done,
					     job,
					     NULL);

	return FALSE;
}

void
nemo_file_operations_move (GList *files,
			       GArray *relative_item_points,
			       GFile *target_dir,
			       GtkWindow *parent_window,
			       NemoCopyCallback  done_callback,
			       gpointer done_callback_data)
{
	CopyMoveJob *job;

	job = op_job_new (CopyMoveJob, parent_window);
	job->is_move = TRUE;
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = g_object_ref (target_dir);
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	inhibit_power_manager ((CommonJob *)job, _("Moving Files"));

	if (!nemo_file_undo_manager_pop_flag ()) {
		GFile* src_dir;

		src_dir = g_file_get_parent (files->data);

		if (g_file_has_uri_scheme (g_list_first (files)->data, "trash")) {
			job->common.undo_info = nemo_file_undo_info_ext_new (NEMO_FILE_UNDO_OP_RESTORE_FROM_TRASH,
										 g_list_length (files),
										 src_dir, target_dir);
		} else {
			job->common.undo_info = nemo_file_undo_info_ext_new (NEMO_FILE_UNDO_OP_MOVE,
										 g_list_length (files),
										 src_dir, target_dir);
		}

		g_object_unref (src_dir);
	}

	g_io_scheduler_push_job (move_job,
				 job,
				 NULL, /* destroy notify */
				 0,
				 job->common.cancellable);
}

static void
report_link_progress (CopyMoveJob *link_job, int total, int left)
{
	CommonJob *job;

	job = (CommonJob *)link_job;
	
	nemo_progress_info_take_status (job->progress,
					    f (_("Creating links in \"%B\""),
					       link_job->destination));

	nemo_progress_info_take_details (job->progress,
					     f (ngettext ("Making link to %'d file",
							  "Making links to %'d files",
							  left), left));

	nemo_progress_info_set_progress (job->progress, left, total);
}

static char *
get_abs_path_for_symlink (GFile *file)
{
	GFile *root, *parent;
	char *relative, *abs;
	
	if (g_file_is_native (file)) {
		return g_file_get_path (file);
	}

	root = g_object_ref (file);
	while ((parent = g_file_get_parent (root)) != NULL) {
		g_object_unref (root);
		root = parent;
	}
	
	relative = g_file_get_relative_path (root, file);
	g_object_unref (root);
	abs = g_strconcat ("/", relative, NULL);
	g_free (relative);
	return abs;
}


static void
link_file (CopyMoveJob *job,
	   GFile *src, GFile *dest_dir,
	   char **dest_fs_type,
	   GHashTable *debuting_files,
	   GdkPoint *position,
	   int files_left)
{
	GFile *src_dir, *dest, *new_dest;
	int count;
	char *path;
	gboolean not_local;
	GError *error;
	CommonJob *common;
	char *primary, *secondary, *details;
	int response;
	gboolean handled_invalid_filename;

	common = (CommonJob *)job;

	count = 0;

	src_dir = g_file_get_parent (src);
	if (g_file_equal (src_dir, dest_dir)) {
		count = 1;
	}
	g_object_unref (src_dir);

	handled_invalid_filename = *dest_fs_type != NULL;

	dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count);

 retry:
	error = NULL;
	not_local = FALSE;
	
	path = get_abs_path_for_symlink (src);
	if (path == NULL) {
		not_local = TRUE;
	} else if (g_file_make_symbolic_link (dest,
					      path, 
					      common->cancellable,
					      &error)) {

		if (common->undo_info != NULL) {
			nemo_file_undo_info_ext_add_origin_target_pair (NEMO_FILE_UNDO_INFO_EXT (common->undo_info),
									    src, dest);
		}

		g_free (path);
		if (debuting_files) {
			g_hash_table_replace (debuting_files, g_object_ref (dest), GINT_TO_POINTER (TRUE));
		}
		
		nemo_file_changes_queue_file_added (dest);
		if (position) {
			nemo_file_changes_queue_schedule_position_set (dest, *position, common->screen_num);
		} else {
			nemo_file_changes_queue_schedule_position_remove (dest);
		}

		g_object_unref (dest);
		
		return;
	}
	g_free (path);

	if (error != NULL &&
	    IS_IO_ERROR (error, INVALID_FILENAME) &&
	    !handled_invalid_filename) {
		handled_invalid_filename = TRUE;

		g_assert (*dest_fs_type == NULL);
		*dest_fs_type = query_fs_type (dest_dir, common->cancellable);

		new_dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count);

		if (!g_file_equal (dest, new_dest)) {
			g_object_unref (dest);
			dest = new_dest;
			g_error_free (error);

			goto retry;
		} else {
			g_object_unref (new_dest);
		}
	}
	/* Conflict */
	if (error != NULL && IS_IO_ERROR (error, EXISTS)) {
		g_object_unref (dest);
		dest = get_target_file_for_link (src, dest_dir, *dest_fs_type, count++);
		g_error_free (error);
		goto retry;
	}

	else if (error != NULL && IS_IO_ERROR (error, CANCELLED)) {
		g_error_free (error);
	}
	
	/* Other error */
	else {
		if (common->skip_all_error) {
			goto out;
		}
		primary = f (_("Error while creating link to %B."), src);
		if (not_local) {
			secondary = f (_("Symbolic links only supported for local files"));
			details = NULL;
		} else if (IS_IO_ERROR (error, NOT_SUPPORTED)) {
			secondary = f (_("The target doesn't support symbolic links."));
			details = NULL;
		} else {
			secondary = f (_("There was an error creating the symlink in %F."), dest_dir);
			details = error->message;
		}
		
		response = run_warning (common,
					primary,
					secondary,
					details,
					files_left > 1,
					GTK_STOCK_CANCEL, SKIP_ALL, SKIP,
					NULL);

		if (error) {
			g_error_free (error);
		}
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (common);
		} else if (response == 1) { /* skip all */
			common->skip_all_error = TRUE;
		} else if (response == 2) { /* skip */
			/* do nothing */
		} else {
			g_assert_not_reached ();
		}
	}
	
 out:
	g_object_unref (dest);
}

static gboolean
link_job_done (gpointer user_data)
{
	CopyMoveJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->debuting_files,
				    !job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}

	g_list_free_full (job->files, g_object_unref);
	g_object_unref (job->destination);
	g_hash_table_unref (job->debuting_files);
	g_free (job->icon_positions);
	
	finalize_common ((CommonJob *)job);

	nemo_file_changes_consume_changes (TRUE);
	return FALSE;
}

static gboolean
link_job (GIOSchedulerJob *io_job,
	  GCancellable *cancellable,
	  gpointer user_data)
{
	CopyMoveJob *job;
	CommonJob *common;
	GFile *src;
	GdkPoint *point;
	char *dest_fs_type;
	int total, left;
	int i;
	GList *l;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	dest_fs_type = NULL;
	
	nemo_progress_info_start (job->common.progress);
	
	verify_destination (&job->common,
			    job->destination,
			    NULL,
			    -1);
	if (job_aborted (common)) {
		goto aborted;
	}

	total = left = g_list_length (job->files);
	
	report_link_progress (job, total, left);

	i = 0;
	for (l = job->files;
	     l != NULL && !job_aborted (common);
	     l = l->next) {
		src = l->data;

		if (i < job->n_icon_positions) {
			point = &job->icon_positions[i];
		} else {
			point = NULL;
		}

		
		link_file (job, src, job->destination,
			   &dest_fs_type, job->debuting_files,
			   point, left);
		report_link_progress (job, total, --left);
		i++;
		
	}

 aborted:
	g_free (dest_fs_type);
	
	g_io_scheduler_job_send_to_mainloop (io_job,
					     link_job_done,
					     job,
					     NULL);

	return FALSE;
}

void
nemo_file_operations_link (GList *files,
			       GArray *relative_item_points,
			       GFile *target_dir,
			       GtkWindow *parent_window,
			       NemoCopyCallback  done_callback,
			       gpointer done_callback_data)
{
	CopyMoveJob *job;

	job = op_job_new (CopyMoveJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = g_object_ref (target_dir);
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	if (!nemo_file_undo_manager_pop_flag ()) {
		GFile* src_dir;

		src_dir = g_file_get_parent (files->data);
		job->common.undo_info = nemo_file_undo_info_ext_new (NEMO_FILE_UNDO_OP_CREATE_LINK,
									 g_list_length (files),
									 src_dir, target_dir);
		g_object_unref (src_dir);
	}

	g_io_scheduler_push_job (link_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}


void
nemo_file_operations_duplicate (GList *files,
				    GArray *relative_item_points,
				    GtkWindow *parent_window,
				    NemoCopyCallback  done_callback,
				    gpointer done_callback_data)
{
	CopyMoveJob *job;

	job = op_job_new (CopyMoveJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->files = eel_g_object_list_copy (files);
	job->destination = NULL;
	if (relative_item_points != NULL &&
	    relative_item_points->len > 0) {
		job->icon_positions =
			g_memdup (relative_item_points->data,
				  sizeof (GdkPoint) * relative_item_points->len);
		job->n_icon_positions = relative_item_points->len;
	}
	job->debuting_files = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal, g_object_unref, NULL);

	if (!nemo_file_undo_manager_pop_flag ()) {
		GFile* src_dir;

		src_dir = g_file_get_parent (files->data);
		job->common.undo_info =
			nemo_file_undo_info_ext_new (NEMO_FILE_UNDO_OP_DUPLICATE,
							 g_list_length (files),
							 src_dir, src_dir);
		g_object_unref (src_dir);
	}

	g_io_scheduler_push_job (copy_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

static gboolean
set_permissions_job_done (gpointer user_data)
{
	SetPermissionsJob *job;

	job = user_data;
	
	g_object_unref (job->file);

	if (job->done_callback) {
		job->done_callback (!job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}
	
	finalize_common ((CommonJob *)job);
	return FALSE;
}

static void
set_permissions_file (SetPermissionsJob *job,
		      GFile *file,
		      GFileInfo *info)
{
	CommonJob *common;
	GFileInfo *child_info;
	gboolean free_info;
	guint32 current;
	guint32 value;
	guint32 mask;
	GFileEnumerator *enumerator;
	GFile *child;
	
	common = (CommonJob *)job;

	nemo_progress_info_pulse_progress (common->progress);
	
	free_info = FALSE;
	if (info == NULL) {
		free_info = TRUE;
		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_TYPE","
					  G_FILE_ATTRIBUTE_UNIX_MODE,
					  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					  common->cancellable,
					  NULL);
		/* Ignore errors */
		if (info == NULL) {
			return;
		}
	}

	if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		value = job->dir_permissions;
		mask = job->dir_mask;
	} else {
		value = job->file_permissions;
		mask = job->file_mask;
	}

	
	if (!job_aborted (common) &&
	    g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE)) {
		current = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

		if (common->undo_info != NULL) {
			nemo_file_undo_info_rec_permissions_add_file (NEMO_FILE_UNDO_INFO_REC_PERMISSIONS (common->undo_info),
									  file, current);
		}

		current = (current & ~mask) | value;

		g_file_set_attribute_uint32 (file, G_FILE_ATTRIBUTE_UNIX_MODE,
					     current, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     common->cancellable, NULL);
	}
	
	if (!job_aborted (common) &&
	    g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
		enumerator = g_file_enumerate_children (file,
							G_FILE_ATTRIBUTE_STANDARD_NAME","
							G_FILE_ATTRIBUTE_STANDARD_TYPE","
							G_FILE_ATTRIBUTE_UNIX_MODE,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							common->cancellable,
							NULL);
		if (enumerator) {
			while (!job_aborted (common) &&
			       (child_info = g_file_enumerator_next_file (enumerator, common->cancellable, NULL)) != NULL) {
				child = g_file_get_child (file,
							  g_file_info_get_name (child_info));
				set_permissions_file (job, child, child_info);
				g_object_unref (child);
				g_object_unref (child_info);
			}
			g_file_enumerator_close (enumerator, common->cancellable, NULL);
			g_object_unref (enumerator);
		} 
	}
	if (free_info) {
		g_object_unref (info);
	}
}


static gboolean
set_permissions_job (GIOSchedulerJob *io_job,
		     GCancellable *cancellable,
		     gpointer user_data)
{
	SetPermissionsJob *job = user_data;
	CommonJob *common;
	
	common = (CommonJob *)job;
	common->io_job = io_job;
	
	nemo_progress_info_set_status (common->progress,
					   _("Setting permissions"));

	nemo_progress_info_start (job->common.progress);

	set_permissions_file (job, job->file, NULL);

	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   set_permissions_job_done,
						   job,
						   NULL);

	return FALSE;
}



void
nemo_file_set_permissions_recursive (const char *directory,
					 guint32         file_permissions,
					 guint32         file_mask,
					 guint32         dir_permissions,
					 guint32         dir_mask,
					 NemoOpCallback  callback,
					 gpointer  callback_data)
{
	SetPermissionsJob *job;
	
	job = op_job_new (SetPermissionsJob, NULL);
	job->file = g_file_new_for_uri (directory);
	job->file_permissions = file_permissions;
	job->file_mask = file_mask;
	job->dir_permissions = dir_permissions;
	job->dir_mask = dir_mask;
	job->done_callback = callback;
	job->done_callback_data = callback_data;

	if (!nemo_file_undo_manager_pop_flag ()) {
		job->common.undo_info = 
			nemo_file_undo_info_rec_permissions_new (job->file,
								     file_permissions, file_mask,
								     dir_permissions, dir_mask);
	}

	g_io_scheduler_push_job (set_permissions_job,
			   job,
			   NULL,
			   0,
			   NULL);
}

static GList *
location_list_from_uri_list (const GList *uris)
{
	const GList *l;
	GList *files;
	GFile *f;

	files = NULL;
	for (l = uris; l != NULL; l = l->next) {
		f = g_file_new_for_uri (l->data);
		files = g_list_prepend (files, f);
	}

	return g_list_reverse (files);
}

typedef struct {
	NemoCopyCallback real_callback;
	gpointer real_data;
} MoveTrashCBData;

static void
callback_for_move_to_trash (GHashTable *debuting_uris,
			    gboolean user_cancelled,
			    MoveTrashCBData *data)
{
	if (data->real_callback)
		data->real_callback (debuting_uris, !user_cancelled, data->real_data);
	g_slice_free (MoveTrashCBData, data);
}

void
nemo_file_operations_copy_move (const GList *item_uris,
				    GArray *relative_item_points,
				    const char *target_dir,
				    GdkDragAction copy_action,
				    GtkWidget *parent_view,
				    NemoCopyCallback  done_callback,
				    gpointer done_callback_data)
{
	GList *locations;
	GList *p;
	GFile *dest, *src_dir;
	GtkWindow *parent_window;
	gboolean target_is_mapping;
	gboolean have_nonmapping_source;
	                        	
	dest = NULL;
	target_is_mapping = FALSE;
	have_nonmapping_source = FALSE;
                
	if (target_dir) {
		dest = g_file_new_for_uri (target_dir);
		if (g_file_has_uri_scheme (dest, "burn")) {
			target_is_mapping = TRUE;
                }
	}

	locations = location_list_from_uri_list (item_uris);
	
	for (p = location_list_from_uri_list (item_uris); p != NULL; p = p->next) {
		if (!g_file_has_uri_scheme ((GFile* )p->data, "burn")) {                
			have_nonmapping_source = TRUE;
		}
	}
	
	if (target_is_mapping && have_nonmapping_source && copy_action == GDK_ACTION_MOVE) {
		/* never move to "burn:///", but fall back to copy.
		 * This is a workaround, because otherwise the source files would be removed.
		 */
		copy_action = GDK_ACTION_COPY;
	}
	
	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}
	
	if (copy_action == GDK_ACTION_COPY) {
		src_dir = g_file_get_parent (locations->data);
		if (target_dir == NULL ||
		    (src_dir != NULL &&
		     g_file_equal (src_dir, dest))) {

			nemo_file_operations_duplicate (locations,
							    relative_item_points,
							    parent_window,
							    done_callback, done_callback_data);
		} else {
			nemo_file_operations_copy (locations,
						       relative_item_points,
						       dest,
						       parent_window,
						       done_callback, done_callback_data);
		}
		if (src_dir) {
			g_object_unref (src_dir);
		}
		
	} else if (copy_action == GDK_ACTION_MOVE) {
		if (g_file_has_uri_scheme (dest, "trash")) {
			MoveTrashCBData *cb_data;
			
			cb_data = g_slice_new0 (MoveTrashCBData);
			cb_data->real_callback = done_callback;
			cb_data->real_data = done_callback_data;

			nemo_file_operations_trash_or_delete (locations,
								  parent_window,
								  (NemoDeleteCallback) callback_for_move_to_trash,
								  cb_data);
		} else {

			nemo_file_operations_move (locations,
						       relative_item_points,
						       dest,
						       parent_window,
						       done_callback, done_callback_data);
		}
	} else {

		nemo_file_operations_link (locations,
					       relative_item_points,
					       dest,
					       parent_window,
					       done_callback, done_callback_data);
	}
	
	g_list_free_full (locations, g_object_unref);
	if (dest) {
		g_object_unref (dest);
	}
}

static gboolean
create_job_done (gpointer user_data)
{
	CreateJob *job;

	job = user_data;
	if (job->done_callback) {
		job->done_callback (job->created_file,
				    !job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}

	g_object_unref (job->dest_dir);
	if (job->src) {
		g_object_unref (job->src);
	}
	g_free (job->src_data);
	g_free (job->filename);
	if (job->created_file) {
		g_object_unref (job->created_file);
	}
	
	finalize_common ((CommonJob *)job);

	nemo_file_changes_consume_changes (TRUE);
	return FALSE;
}

static gboolean
create_job (GIOSchedulerJob *io_job,
	    GCancellable *cancellable,
	    gpointer user_data)
{
	CreateJob *job;
	CommonJob *common;
	int count;
	GFile *dest;
	char *basename;
	char *filename, *filename2, *new_filename;
	char *filename_base, *suffix;
	char *dest_fs_type;
	GError *error;
	gboolean res;
	gboolean filename_is_utf8;
	char *primary, *secondary, *details;
	int response;
	char *data;
	int length;
	GFileOutputStream *out;
	gboolean handled_invalid_filename;
	int max_length, offset;

	job = user_data;
	common = &job->common;
	common->io_job = io_job;

	nemo_progress_info_start (job->common.progress);

	handled_invalid_filename = FALSE;

	dest_fs_type = NULL;
	filename = NULL;
	dest = NULL;

	max_length = get_max_name_length (job->dest_dir);

	verify_destination (common,
			    job->dest_dir,
			    NULL, -1);
	if (job_aborted (common)) {
		goto aborted;
	}

	filename = g_strdup (job->filename);
	filename_is_utf8 = FALSE;
	if (filename) {
		filename_is_utf8 = g_utf8_validate (filename, -1, NULL);		
	}
	if (filename == NULL) {
		if (job->make_dir) {
			/* localizers: the initial name of a new folder  */
			filename = g_strdup (_("Untitled Folder"));
			filename_is_utf8 = TRUE; /* Pass in utf8 */
		} else {
			if (job->src != NULL) {
				basename = g_file_get_basename (job->src);
				/* localizers: the initial name of a new template document */
				filename = g_strdup_printf (_("Untitled %s"), basename);

				g_free (basename);
			}
			if (filename == NULL) {
				/* localizers: the initial name of a new empty document */
				filename = g_strdup (_("Untitled Document"));
				filename_is_utf8 = TRUE; /* Pass in utf8 */
			}
		}
	}

	make_file_name_valid_for_dest_fs (filename, dest_fs_type);
	if (filename_is_utf8) {
		dest = g_file_get_child_for_display_name (job->dest_dir, filename, NULL);
	}
	if (dest == NULL) {
		dest = g_file_get_child (job->dest_dir, filename);
	}
	count = 1;

 retry:

	error = NULL;
	if (job->make_dir) {
		res = g_file_make_directory (dest,
					     common->cancellable,
					     &error);

		if (res && common->undo_info != NULL) {
			nemo_file_undo_info_create_set_data (NEMO_FILE_UNDO_INFO_CREATE (common->undo_info),
								 dest, NULL, 0);
		}

	} else {
		if (job->src) {
			res = g_file_copy (job->src,
					   dest,
					   G_FILE_COPY_NONE,
					   common->cancellable,
					   NULL, NULL,
					   &error);

			if (res && common->undo_info != NULL) {
				gchar *uri;

				uri = g_file_get_uri (job->src);
				nemo_file_undo_info_create_set_data (NEMO_FILE_UNDO_INFO_CREATE (common->undo_info),
									 dest, uri, 0);

				g_free (uri);
			}

		} else {
			data = "";
			length = 0;
			if (job->src_data) {
				data = job->src_data;
				length = job->length;
			}

			out = g_file_create (dest,
					     G_FILE_CREATE_NONE,
					     common->cancellable,
					     &error);
			if (out) {
				res = g_output_stream_write_all (G_OUTPUT_STREAM (out),
								 data, length,
								 NULL,
								 common->cancellable,
								 &error);
				if (res) {
					res = g_output_stream_close (G_OUTPUT_STREAM (out),
								     common->cancellable,
								     &error);

					if (res && common->undo_info != NULL) {
						nemo_file_undo_info_create_set_data (NEMO_FILE_UNDO_INFO_CREATE (common->undo_info),
											 dest, data, length);
					}
				}

				/* This will close if the write failed and we didn't close */
				g_object_unref (out);
			} else {
				res = FALSE;
			}
		}
	}

	if (res) {
		job->created_file = g_object_ref (dest);
		nemo_file_changes_queue_file_added (dest);
		if (job->has_position) {
			nemo_file_changes_queue_schedule_position_set (dest, job->position, common->screen_num);
		} else {
			nemo_file_changes_queue_schedule_position_remove (dest);
		}
	} else {
		g_assert (error != NULL);

		if (IS_IO_ERROR (error, INVALID_FILENAME) &&
		    !handled_invalid_filename) {
			handled_invalid_filename = TRUE;

			g_assert (dest_fs_type == NULL);
			dest_fs_type = query_fs_type (job->dest_dir, common->cancellable);

			g_object_unref (dest);

			if (count == 1) {
				new_filename = g_strdup (filename);
			} else {
				filename_base = eel_filename_strip_extension (filename);
				offset = strlen (filename_base);
				suffix = g_strdup (filename + offset);

				filename2 = g_strdup_printf ("%s %d%s", filename_base, count, suffix);

				new_filename = NULL;
				if (max_length > 0 && strlen (filename2) > max_length) {
					new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
				}

				if (new_filename == NULL) {
					new_filename = g_strdup (filename2);
				}

				g_free (filename2);
				g_free (suffix);
			}

			if (make_file_name_valid_for_dest_fs (new_filename, dest_fs_type)) {
				g_object_unref (dest);

				if (filename_is_utf8) {
					dest = g_file_get_child_for_display_name (job->dest_dir, new_filename, NULL);
				}
				if (dest == NULL) {
					dest = g_file_get_child (job->dest_dir, new_filename);
				}

				g_free (new_filename);
				g_error_free (error);
				goto retry;
			}
			g_free (new_filename);
		} else if (IS_IO_ERROR (error, EXISTS)) {
			g_object_unref (dest);
			dest = NULL;
			filename_base = eel_filename_strip_extension (filename);
			offset = strlen (filename_base);
			suffix = g_strdup (filename + offset);

			filename2 = g_strdup_printf ("%s %d%s", filename_base, ++count, suffix);

			if (max_length > 0 && strlen (filename2) > max_length) {
				new_filename = shorten_utf8_string (filename2, strlen (filename2) - max_length);
				if (new_filename != NULL) {
					g_free (filename2);
					filename2 = new_filename;
				}
			}

			make_file_name_valid_for_dest_fs (filename2, dest_fs_type);
			if (filename_is_utf8) {
				dest = g_file_get_child_for_display_name (job->dest_dir, filename2, NULL);
			}
			if (dest == NULL) {
				dest = g_file_get_child (job->dest_dir, filename2);
			}
			g_free (filename2);
			g_free (suffix);
			g_error_free (error);
			goto retry;
		}
		
		else if (IS_IO_ERROR (error, CANCELLED)) {
			g_error_free (error);
		}
		
		/* Other error */
		else {
			if (job->make_dir) {
				primary = f (_("Error while creating directory %B."), dest);
			} else {
				primary = f (_("Error while creating file %B."), dest);
			}
			secondary = f (_("There was an error creating the directory in %F."), job->dest_dir);
			details = error->message;
		
			response = run_warning (common,
						primary,
						secondary,
						details,
						FALSE,
						GTK_STOCK_CANCEL, SKIP,
						NULL);
			
			g_error_free (error);
		
			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (common);
			} else if (response == 1) { /* skip */
				/* do nothing */
			} else {
				g_assert_not_reached ();
			}
		}
	}

 aborted:
	if (dest) {
		g_object_unref (dest);
	}
	g_free (filename);
	g_free (dest_fs_type);
	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   create_job_done,
						   job,
						   NULL);

	return FALSE;
}

void 
nemo_file_operations_new_folder (GtkWidget *parent_view, 
				     GdkPoint *target_point,
				     const char *parent_dir,
				     NemoCreateCallback done_callback,
				     gpointer done_callback_data)
{
	CreateJob *job;
	GtkWindow *parent_window;

	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}

	job = op_job_new (CreateJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->dest_dir = g_file_new_for_uri (parent_dir);
	job->make_dir = TRUE;
	if (target_point != NULL) {
		job->position = *target_point;
		job->has_position = TRUE;
	}

	if (!nemo_file_undo_manager_pop_flag ()) {
		job->common.undo_info = nemo_file_undo_info_create_new (NEMO_FILE_UNDO_OP_CREATE_FOLDER);
	}

	g_io_scheduler_push_job (create_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

void 
nemo_file_operations_new_file_from_template (GtkWidget *parent_view, 
						 GdkPoint *target_point,
						 const char *parent_dir,
						 const char *target_filename,
						 const char *template_uri,
						 NemoCreateCallback done_callback,
						 gpointer done_callback_data)
{
	CreateJob *job;
	GtkWindow *parent_window;

	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}

	job = op_job_new (CreateJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->dest_dir = g_file_new_for_uri (parent_dir);
	if (target_point != NULL) {
		job->position = *target_point;
		job->has_position = TRUE;
	}
	job->filename = g_strdup (target_filename);

	if (template_uri) {
		job->src = g_file_new_for_uri (template_uri);
	}

	if (!nemo_file_undo_manager_pop_flag ()) {
		job->common.undo_info = nemo_file_undo_info_create_new (NEMO_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE);
	}

	g_io_scheduler_push_job (create_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}

void 
nemo_file_operations_new_file (GtkWidget *parent_view, 
				   GdkPoint *target_point,
				   const char *parent_dir,
				   const char *target_filename,
				   const char *initial_contents,
				   int length,
				   NemoCreateCallback done_callback,
				   gpointer done_callback_data)
{
	CreateJob *job;
	GtkWindow *parent_window;

	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}

	job = op_job_new (CreateJob, parent_window);
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	job->dest_dir = g_file_new_for_uri (parent_dir);
	if (target_point != NULL) {
		job->position = *target_point;
		job->has_position = TRUE;
	}
	job->src_data = g_memdup (initial_contents, length);
	job->length = length;
	job->filename = g_strdup (target_filename);

	if (!nemo_file_undo_manager_pop_flag ()) {
		job->common.undo_info = nemo_file_undo_info_create_new (NEMO_FILE_UNDO_OP_CREATE_EMPTY_FILE);
	}

	g_io_scheduler_push_job (create_job,
			   job,
			   NULL, /* destroy notify */
			   0,
			   job->common.cancellable);
}



static void
delete_trash_file (CommonJob *job,
		   GFile *file,
		   gboolean del_file,
		   gboolean del_children)
{
	GFileInfo *info;
	GFile *child;
	GFileEnumerator *enumerator;

	if (job_aborted (job)) {
		return;
	}

	if (del_children) {
		enumerator = g_file_enumerate_children (file,
							G_FILE_ATTRIBUTE_STANDARD_NAME ","
							G_FILE_ATTRIBUTE_STANDARD_TYPE,
							G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
							job->cancellable,
							NULL);
		if (enumerator) {
			while (!job_aborted (job) &&
			       (info = g_file_enumerator_next_file (enumerator, job->cancellable, NULL)) != NULL) {
				child = g_file_get_child (file,
							  g_file_info_get_name (info));
				delete_trash_file (job, child, TRUE,
						   g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY);
				g_object_unref (child);
				g_object_unref (info);
			}
			g_file_enumerator_close (enumerator, job->cancellable, NULL);
			g_object_unref (enumerator);
		}
	}

	if (!job_aborted (job) && del_file) {
		g_file_delete (file, job->cancellable, NULL);
	}
}

static gboolean
empty_trash_job_done (gpointer user_data)
{
	EmptyTrashJob *job;

	job = user_data;
	
	g_list_free_full (job->trash_dirs, g_object_unref);

	if (job->done_callback) {
		job->done_callback (!job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}
	
	finalize_common ((CommonJob *)job);
	return FALSE;
}

static gboolean
empty_trash_job (GIOSchedulerJob *io_job,
		 GCancellable *cancellable,
		 gpointer user_data)
{
	EmptyTrashJob *job = user_data;
	CommonJob *common;
	GList *l;
	gboolean confirmed;
	
	common = (CommonJob *)job;
	common->io_job = io_job;
	
	nemo_progress_info_start (job->common.progress);

	if (job->should_confirm) {
		confirmed = confirm_empty_trash (common);
	} else {
		confirmed = TRUE;
	}
	if (confirmed) {
		for (l = job->trash_dirs;
		     l != NULL && !job_aborted (common);
		     l = l->next) {
			delete_trash_file (common, l->data, FALSE, TRUE);
		}
	}

	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   empty_trash_job_done,
						   job,
						   NULL);

	return FALSE;
}

void 
nemo_file_operations_empty_trash (GtkWidget *parent_view)
{
	EmptyTrashJob *job;
	GtkWindow *parent_window;

	parent_window = NULL;
	if (parent_view) {
		parent_window = (GtkWindow *)gtk_widget_get_ancestor (parent_view, GTK_TYPE_WINDOW);
	}

	job = op_job_new (EmptyTrashJob, parent_window);
	job->trash_dirs = g_list_prepend (job->trash_dirs,
					  g_file_new_for_uri ("trash:"));
	job->should_confirm = TRUE;

	inhibit_power_manager ((CommonJob *)job, _("Emptying Trash"));
	
	g_io_scheduler_push_job (empty_trash_job,
			   job,
			   NULL,
			   0,
			   NULL);
}

static gboolean
mark_trusted_job_done (gpointer user_data)
{
	MarkTrustedJob *job = user_data;
	
	g_object_unref (job->file);

	if (job->done_callback) {
		job->done_callback (!job_aborted ((CommonJob *) job),
				    job->done_callback_data);
	}
	
	finalize_common ((CommonJob *)job);
	return FALSE;
}

#define TRUSTED_SHEBANG "#!/usr/bin/env xdg-open\n"

static void
mark_desktop_file_trusted (CommonJob *common,
			   GCancellable *cancellable,
			   GFile *file,
			   gboolean interactive)
{
	char *contents, *new_contents;
	gsize length, new_length;
	GError *error;
	guint32 current_perms, new_perms;
	int response;
	GFileInfo *info;
	
 retry:
	error = NULL;
	if (!g_file_load_contents (file,
				  cancellable,
				  &contents, &length,
				  NULL, &error)) {
		if (interactive) {
			response = run_error (common,
					      g_strdup (_("Unable to mark launcher trusted (executable)")),
					      error->message,
					      NULL,
					      FALSE,
					      GTK_STOCK_CANCEL, RETRY,
					      NULL);
		} else {
			response = 0;
		}
		

		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (common);
		} else if (response == 1) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}

		goto out;
	}

	if (!g_str_has_prefix (contents, "#!")) {
		new_length = length + strlen (TRUSTED_SHEBANG);
		new_contents = g_malloc (new_length);
		
		strcpy (new_contents, TRUSTED_SHEBANG);
		memcpy (new_contents + strlen (TRUSTED_SHEBANG),
			contents, length);
		
		if (!g_file_replace_contents (file,
					      new_contents,
					      new_length,
					      NULL,
					      FALSE, 0,
					      NULL, cancellable, &error)) {
			g_free (contents);
			g_free (new_contents);
			
			if (interactive) {
				response = run_error (common,
						      g_strdup (_("Unable to mark launcher trusted (executable)")),
						      error->message,
						      NULL,
						      FALSE,
						      GTK_STOCK_CANCEL, RETRY,
						      NULL);
			} else {
				response = 0;
			}

			if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
				abort_job (common);
			} else if (response == 1) {
				goto retry;
			} else {
				g_assert_not_reached ();
			}
			
			goto out;
		}
		g_free (new_contents);
		
	}
	g_free (contents);
	
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE","
				  G_FILE_ATTRIBUTE_UNIX_MODE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  common->cancellable,
				  &error);

	if (info == NULL) {
		if (interactive) {
			response = run_error (common,
					      g_strdup (_("Unable to mark launcher trusted (executable)")),
					      error->message,
					      NULL,
					      FALSE,
					      GTK_STOCK_CANCEL, RETRY,
					      NULL);
		} else {
			response = 0;
		}
		
		if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
			abort_job (common);
		} else if (response == 1) {
			goto retry;
		} else {
			g_assert_not_reached ();
		}
		
		goto out;
	}
	
	
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE)) {
		current_perms = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
		new_perms = current_perms | S_IXGRP | S_IXUSR | S_IXOTH;

		if ((current_perms != new_perms) &&
		    !g_file_set_attribute_uint32 (file, G_FILE_ATTRIBUTE_UNIX_MODE,
						  new_perms, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						  common->cancellable, &error))
			{
				g_object_unref (info);
				
				if (interactive) {
					response = run_error (common,
							      g_strdup (_("Unable to mark launcher trusted (executable)")),
							      error->message,
							      NULL,
							      FALSE,
							      GTK_STOCK_CANCEL, RETRY,
							      NULL);
				} else {
					response = 0;
				}
				
				if (response == 0 || response == GTK_RESPONSE_DELETE_EVENT) {
					abort_job (common);
				} else if (response == 1) {
					goto retry;
				} else {
					g_assert_not_reached ();
				}
				
				goto out;
			}
	} 
	g_object_unref (info);
 out:
	;
}

static gboolean
mark_trusted_job (GIOSchedulerJob *io_job,
		  GCancellable *cancellable,
		  gpointer user_data)
{
	MarkTrustedJob *job = user_data;
	CommonJob *common;
	
	common = (CommonJob *)job;
	common->io_job = io_job;
	
	nemo_progress_info_start (job->common.progress);

	mark_desktop_file_trusted (common,
				   cancellable,
				   job->file,
				   job->interactive);
	
	g_io_scheduler_job_send_to_mainloop_async (io_job,
						   mark_trusted_job_done,
						   job,
						   NULL);

	return FALSE;
}

void
nemo_file_mark_desktop_file_trusted (GFile *file,
					 GtkWindow *parent_window,
					 gboolean interactive,
					 NemoOpCallback done_callback,
					 gpointer done_callback_data)
{
	MarkTrustedJob *job;
	
	job = op_job_new (MarkTrustedJob, parent_window);
	job->file = g_object_ref (file);
	job->interactive = interactive;
	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;
	
	g_io_scheduler_push_job (mark_trusted_job,
				 job,
				 NULL,
				 0,
				 NULL);
}

#if !defined (NEMO_OMIT_SELF_CHECK)

void
nemo_self_check_file_operations (void)
{
	setlocale (LC_MESSAGES, "C");

	
	/* test the next duplicate name generator */
	EEL_CHECK_STRING_RESULT (get_duplicate_name (" (copy)", 1, -1), " (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo", 1, -1), "foo (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name (".bashrc", 1, -1), ".bashrc (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name (".foo.txt", 1, -1), ".foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo", 1, -1), "foo foo (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo.txt", 1, -1), "foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt", 1, -1), "foo foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo.txt txt", 1, -1), "foo foo (copy).txt txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...txt", 1, -1), "foo.. (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo...", 1, -1), "foo... (copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo. (copy)", 1, -1), "foo. (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy)", 1, -1), "foo (another copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (copy).txt", 1, -1), "foo (another copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy)", 1, -1), "foo (3rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (another copy).txt", 1, -1), "foo (3rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (another copy).txt", 1, -1), "foo foo (3rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy)", 1, -1), "foo (14th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (13th copy).txt", 1, -1), "foo (14th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy)", 1, -1), "foo (22nd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (21st copy).txt", 1, -1), "foo (22nd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy)", 1, -1), "foo (23rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (22nd copy).txt", 1, -1), "foo (23rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy)", 1, -1), "foo (24th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (23rd copy).txt", 1, -1), "foo (24th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy)", 1, -1), "foo (25th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (24th copy).txt", 1, -1), "foo (25th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy)", 1, -1), "foo foo (25th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (24th copy).txt", 1, -1), "foo foo (25th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo foo (100000000000000th copy).txt", 1, -1), "foo foo (copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy)", 1, -1), "foo (11th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (10th copy).txt", 1, -1), "foo (11th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy)", 1, -1), "foo (12th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (11th copy).txt", 1, -1), "foo (12th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy)", 1, -1), "foo (13th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (12th copy).txt", 1, -1), "foo (13th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy)", 1, -1), "foo (111th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (110th copy).txt", 1, -1), "foo (111th copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy)", 1, -1), "foo (123rd copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (122nd copy).txt", 1, -1), "foo (123rd copy).txt");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy)", 1, -1), "foo (124th copy)");
	EEL_CHECK_STRING_RESULT (get_duplicate_name ("foo (123rd copy).txt", 1, -1), "foo (124th copy).txt");

	setlocale (LC_MESSAGES, "");
}

#endif
