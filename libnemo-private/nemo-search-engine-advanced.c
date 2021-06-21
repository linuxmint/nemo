/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 */

#include <config.h>
#include "nemo-file.h"
#include "nemo-directory.h"
#include "nemo-file-utilities.h"
#include "nemo-search-engine-advanced.h"
#include "nemo-global-preferences.h"

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define DEBUG_FLAG NEMO_DEBUG_SEARCH
#include "nemo-debug.h"

#define SEARCH_HELPER_GROUP "Nemo Search Helper"

#define FILE_SEARCH_ONLY_BATCH_SIZE 500
#define CONTENT_SEARCH_BATCH_SIZE 1
#define SNIPPET_EXTEND_SIZE 100

typedef struct {
    gchar *exec_format;
    gint priority;
    /* future? */
} SearchHelper;

typedef struct {
	NemoSearchEngineAdvanced *engine;
	GCancellable *cancellable;

	GList *mime_types;
	gchar **words;
	gboolean *word_strstr;
	gboolean words_and;

	GQueue *directories; /* GFiles */

	GHashTable *visited;
    GHashTable *skip_folders;

	gint n_processed_files;
    GRegex *match_re;
    GRegex *newline_re;

    GMutex hit_list_lock;
    GList *hit_list; // holds SearchHitData

    gboolean show_hidden;
    gboolean count_hits;
    gboolean recurse;
    gboolean file_case_sensitive;
    gboolean location_supports_content_search;

    GTimer *timer;
} SearchThreadData;

struct NemoSearchEngineAdvancedDetails {
	NemoQuery *query;

	SearchThreadData *active_search;

	gboolean query_finished;
};

G_DEFINE_TYPE (NemoSearchEngineAdvanced, nemo_search_engine_advanced,
	       NEMO_TYPE_SEARCH_ENGINE);

static GHashTable *search_helpers = NULL;

static void
search_helper_free (SearchHelper *helper)
{
    g_free (helper->exec_format);
    g_slice_free (SearchHelper, helper);
}

static GList *
get_cat_helper_directories (void)
{
    gchar **data_dirs;
    gchar *path;
    guint i;
    GList *helper_dirs = NULL;

    data_dirs = (gchar **) g_get_system_data_dirs ();

    for (i = 0; i < g_strv_length (data_dirs); i++) {
        path = g_build_filename (data_dirs[i], "nemo", "search-helpers", NULL);

        if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
            g_free (path);
            continue;
        }

        helper_dirs = g_list_prepend (helper_dirs, path);
    }

    path = g_build_filename (g_get_user_data_dir (), "nemo", "search-helpers", NULL);

    if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents (path, DEFAULT_NEMO_DIRECTORY_MODE);
    }

    helper_dirs = g_list_prepend (helper_dirs, path);
    helper_dirs = g_list_reverse (helper_dirs);

    return helper_dirs;
}

static void
process_search_helper_file (const gchar *path)
{
    GKeyFile *key_file;
    gchar *exec_format = NULL;
    gchar **try_exec_list = NULL;
    gchar *abs_try_path = NULL;
    gchar **mime_types = NULL;
    gint priority = 100;
    gsize n_types;
    gint i;

    DEBUG ("Loading search helper: %s", path);

    key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL);

    if (!g_key_file_has_group (key_file, SEARCH_HELPER_GROUP)) {
        g_warning ("Nemo search_helper file is missing group '%s' - %s", SEARCH_HELPER_GROUP, path);
        goto done;
    }

    if (!g_key_file_has_key (key_file, SEARCH_HELPER_GROUP, "TryExec", NULL) ||
        !g_key_file_has_key (key_file, SEARCH_HELPER_GROUP, "Exec", NULL) ||
        !g_key_file_has_key (key_file, SEARCH_HELPER_GROUP, "MimeType", NULL)) {
        g_warning ("Nemo search_helper file is missing mandatory fields - you must have TryExec, Exec and MimeType, "
                   "and MimeType list must terminate with a ; - %s", path);
        goto done;
    }

    try_exec_list = g_key_file_get_string_list (key_file, SEARCH_HELPER_GROUP, "TryExec", NULL, NULL);
    gboolean try_failed = FALSE;

    for (i = 0; i < g_strv_length (try_exec_list); i++) {
        abs_try_path = g_find_program_in_path (try_exec_list[i]);

        if (!abs_try_path) {
            DEBUG ("Skipping search helper '%s' - program is not available (%s)", path, try_exec_list[i]);
            try_failed = TRUE;
            break;
        }

        g_free (abs_try_path);
    }

    g_strfreev (try_exec_list);

    if (try_failed) {
        goto done;
    }

    n_types = 0;
    mime_types = g_key_file_get_string_list (key_file, SEARCH_HELPER_GROUP, "MimeType", &n_types, NULL);

    if (n_types == 0) {
        g_warning ("Nemo search_helper no mimetypes defined - %s", path);
        goto done;
    }

    exec_format = g_key_file_get_string (key_file, SEARCH_HELPER_GROUP, "Exec", NULL);

    if (exec_format == NULL) {
        g_warning ("Nemo search_helper could not retrieve Exec field - %s", path);
        goto done;
    }

    if (g_key_file_has_key (key_file, SEARCH_HELPER_GROUP, "Priority", NULL)) {
        priority = g_key_file_get_integer (key_file, SEARCH_HELPER_GROUP, "Priority", NULL);

        // Failure sets the return to 0, make it 100 for the default when there's no key.
        if (priority == 0) {
            priority = 100;
        }
    }

    /* The helper table is keyed to mimetype strings, which will point to the same value */

    for (i = 0; i < n_types; i++) {
        SearchHelper *helper, *existing;
        const gchar *mime_type;

        mime_type = mime_types[i];

        existing = g_hash_table_lookup (search_helpers, mime_type);
        if (existing && existing->priority > priority) {
            DEBUG ("Existing nemo search_helper for '%s' has higher priority than a new one (%s), ignoring the new one.", mime_type, path);
            continue;
        } else if (existing) {
            DEBUG ("Replacing existing nemo search_helper for '%s' with %s based on priority.", mime_type, path);
        }

        helper = g_slice_new0 (SearchHelper);
        helper->exec_format = g_strdup (exec_format);
        helper->priority = priority;

        g_hash_table_replace (search_helpers, g_strdup (mime_type), helper);
    }

done:
    g_key_file_free (key_file);
    g_free (exec_format);

    if (mime_types != NULL) {
        g_strfreev (mime_types);
    }
}

static void
initialize_search_helpers (NemoSearchEngineAdvanced *engine)
{
    GList *dir_list, *d_iter;

    search_helpers = g_hash_table_new_full (g_str_hash, g_str_equal,
                                            g_free, (GDestroyNotify) search_helper_free);

    dir_list = get_cat_helper_directories ();

    for (d_iter = dir_list; d_iter != NULL; d_iter = d_iter->next) {
        GError *error;
        GDir *dir;
        const gchar *filename;
        const gchar *path;

        path = (const gchar *) d_iter->data;
        DEBUG ("Checking location '%s' for search helpers", path);

        error = NULL;

        dir = g_dir_open (path, 0, &error);
        if (error != NULL) {
            g_warning ("Could not load helper dir (%s): %s", path, error->message);
            g_clear_error (&error);
            continue;
        }

        while ((filename = g_dir_read_name (dir)) != NULL) {
            gchar *file_path;

            if (!g_str_has_suffix (filename, ".nemo_search_helper")) {
                continue;
            }

            file_path = g_build_filename (path, filename, NULL);
            DEBUG ("Processing '%s'", path);
            process_search_helper_file (file_path);
            g_free (file_path);
        }

        g_dir_close (dir);
    }

    g_list_free_full (dir_list, g_free);
}

void free_search_helpers (void)
{
    if (search_helpers != NULL) {
        g_hash_table_destroy (search_helpers);
        search_helpers = NULL;
    }
}

static void
finalize (GObject *object)
{
	NemoSearchEngineAdvanced *simple;

	simple = NEMO_SEARCH_ENGINE_ADVANCED (object);

	if (simple->details->query) {
		g_object_unref (simple->details->query);
		simple->details->query = NULL;
	}

	G_OBJECT_CLASS (nemo_search_engine_advanced_parent_class)->finalize (object);
}

/**
 * function modified taken from glib2 / gstrfuncs.c
 */
static gchar**
strsplit_esc_n (const gchar *string,
				const gchar delimiter,
				const gchar escape,
				gint max_tokens,
				gint *n_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array;
	guint n = 0;
	const gchar *remainder, *s;

	g_return_val_if_fail (string != NULL, NULL);
	g_return_val_if_fail (delimiter != '\0', NULL);

	if (max_tokens < 1)
	max_tokens = G_MAXINT;

	remainder = string;
	s = remainder;
	while (s && *s) {
		if (*s == delimiter) break;
		else if (*s == escape) {
			s++;
			if (*s == 0) break;
		}
		s++;
	}
	if (*s == 0) s = NULL;
	if (s) {
		while (--max_tokens && s) {
			gsize len;

			len = s - remainder;
			string_list = g_slist_prepend (string_list,
										 g_strndup (remainder, len));
			n++;
			remainder = s + 1;

			s = remainder;
			while (s && *s) {
				if (*s == delimiter) break;
				else if (*s == escape) {
					s++;
					if (*s == 0) break;
				}
				s++;
			}
			if (*s == 0) s = NULL;
		}
	}
	if (*string) {
		n++;
		string_list = g_slist_prepend (string_list, g_strdup (remainder));
	}
	*n_tokens = n;
	str_array = g_new (gchar*, n + 1);

	str_array[n--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[n--] = slist->data;

	g_slist_free (string_list);

	return str_array;
}

static SearchThreadData *
search_thread_data_new (NemoSearchEngineAdvanced *engine,
			NemoQuery *query)
{
    GRegexCompileFlags flags;
    GError *error;
    SearchThreadData *data;
    char *text, *cased, *normalized, *uri;
    GFile *location;
    gint n = 1, i;
    gchar *format;

	data = g_new0 (SearchThreadData, 1);

    data->show_hidden = nemo_query_get_show_hidden (query);
	data->engine = engine;
	data->directories = g_queue_new ();
	data->visited = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	uri = nemo_query_get_location (query);
	location = NULL;
	if (uri != NULL) {
		location = g_file_new_for_uri (uri);
        data->location_supports_content_search = g_file_is_native (location);
		g_free (uri);
	}
	if (location == NULL) {
		location = g_file_new_for_path ("/");
	}
	g_queue_push_tail (data->directories, location);

	text = nemo_query_get_file_pattern (query);
	normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);

    data->file_case_sensitive = nemo_query_get_file_case_sensitive (query);

    if (!data->file_case_sensitive) {
        cased = g_utf8_strdown (normalized, -1);
    } else {
        cased = g_strdup (normalized);
    }

	data->words = strsplit_esc_n (cased, ' ', '\\', -1, &n);
    g_free (text);
	g_free (cased);
	g_free (normalized);

	data->word_strstr = g_malloc(sizeof(gboolean)*n);
	data->words_and = TRUE;
	for (i = 0; data->words[i] != NULL; i++) {
		data->word_strstr[i]=TRUE;
		text = data->words[i];
		while(*text!=0) {
			if(*text=='\\' || *text=='?' || *text=='*') {
				data->word_strstr[i]=FALSE;
				break;
			}
			text++;
		}
		if (!data->word_strstr[i]) data->words_and = FALSE;
	}

    data->skip_folders = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    gchar **folders_array = g_settings_get_strv (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_SKIP_FOLDERS);
    for (i = 0; i < g_strv_length (folders_array); i++) {
        DEBUG ("Ignoring folder in search: '%s'", folders_array[i]);
        g_hash_table_add (data->skip_folders, g_strdup (folders_array[i]));
    }
    g_strfreev (folders_array);

    data->count_hits = FALSE;

    gchar **saved_search_columns = g_settings_get_strv (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_VISIBLE_COLUMNS);
    if (g_strv_contains ((const gchar * const *) saved_search_columns, "search_result_count")) {
        data->count_hits = TRUE;
        DEBUG ("Counting search hits");
    } else {
        DEBUG ("Not counting search hits");
    }
    g_strfreev (saved_search_columns);

	data->mime_types = nemo_query_get_mime_types (query);
    data->recurse = nemo_query_get_recurse (query);
    data->file_case_sensitive = nemo_query_get_file_case_sensitive (query);

	data->cancellable = g_cancellable_new ();
    data->timer = g_timer_new ();

    g_mutex_init (&data->hit_list_lock);

    gchar *content_pattern = nemo_query_get_content_pattern (query);

    if (content_pattern != NULL) {
        gchar *escaped;

        if (nemo_query_get_use_regex (query)) {
            escaped = g_strdup (content_pattern);
        } else {
            escaped = g_regex_escape_string (content_pattern, -1);
        }

        flags = G_REGEX_MULTILINE |
                G_REGEX_OPTIMIZE;

        format = g_settings_get_string (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_REGEX_FORMAT);

        if (g_strcmp0 (format, "javascript") == 0) {
            flags |= G_REGEX_JAVASCRIPT_COMPAT;
        }

        if (g_settings_get_boolean (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_USE_RAW)) {
            flags |= G_REGEX_RAW;
        }

        if (!nemo_query_get_content_case_sensitive (query)) {
            flags |= G_REGEX_CASELESS;
        }

        g_free (format);

        error = NULL;

        data->match_re= g_regex_new (escaped,
                                    flags,
                                    0,
                                    &error);

        if (data->match_re == NULL) {
            if (error != NULL) {
                // TODO: Maybe do something in the ui, make the info bar red?
                g_warning ("Pattern /%s/ is invalid: code %d - %s", escaped, error->code, error->message);
            }
            g_clear_error (&error);
        } else {
            DEBUG ("regex is '%s'", g_regex_get_pattern (data->match_re));
        }

        g_free (escaped);

        data->newline_re = g_regex_new ("\\n{2,}",
                                           G_REGEX_OPTIMIZE,
                                           0,
                                           &error);

        if (data->newline_re == NULL) {
            if (error != NULL) {
                // TODO: Maybe do something in the ui, make the info bar red?
                g_warning ("Whitespace match regex is invalid: code %d - %s", error->code, error->message);
            }
            g_clear_error (&error);
        }
    }

    g_free (content_pattern);

	return data;
}

static void
search_thread_data_free (SearchThreadData *data)
{
	g_queue_foreach (data->directories,
			 (GFunc)g_object_unref, NULL);
	g_queue_free (data->directories);
	g_hash_table_destroy (data->visited);
    g_hash_table_destroy (data->skip_folders);
	g_object_unref (data->cancellable);
	g_strfreev (data->words);
	g_free (data->word_strstr);
	g_list_free_full (data->mime_types, g_free);
	g_list_free_full (data->hit_list, (GDestroyNotify) file_search_result_free);
    g_clear_pointer (&data->match_re, g_regex_unref);
    g_clear_pointer (&data->newline_re, g_regex_unref);
    g_timer_destroy (data->timer);
    g_mutex_clear (&data->hit_list_lock);

    g_free (data);
}

static gboolean
search_thread_done_idle (gpointer user_data)
{
	SearchThreadData *data;

	data = user_data;

	if (!g_cancellable_is_cancelled (data->cancellable)) {
		nemo_search_engine_finished (NEMO_SEARCH_ENGINE (data->engine));
		data->engine->details->active_search = NULL;
	}

    DEBUG ("Search took: %f seconds", g_timer_elapsed (data->timer, NULL));
	search_thread_data_free (data);

	return FALSE;
}

typedef struct {
	GList *hit_list;
	SearchThreadData *thread_data;
} SearchHits;


static gboolean
search_thread_add_hits_idle (gpointer user_data)
{
	SearchHits *hits;

	hits = user_data;

    if (!g_cancellable_is_cancelled (hits->thread_data->cancellable)) {
        nemo_search_engine_hits_added (NEMO_SEARCH_ENGINE (hits->thread_data->engine),
                                       hits->hit_list);
    }

    g_list_free_full (hits->hit_list, (GDestroyNotify) file_search_result_free);
	g_free (hits);

	return FALSE;
}

static void
send_batch (SearchThreadData *data)
{
	SearchHits *hits;

    g_mutex_lock (&data->hit_list_lock);
	data->n_processed_files = 0;

	if (data->hit_list) {
		hits = g_new0 (SearchHits, 1);
		hits->hit_list = data->hit_list;
		hits->thread_data = data;
		g_idle_add (search_thread_add_hits_idle, hits);
	}
	data->hit_list = NULL;
    g_mutex_unlock (&data->hit_list_lock);
}

static gboolean
strwildcardcmp(char *a, char *b)
{
    if (*a == 0 && *b == 0)  return TRUE;
    while(*a!=0 && *b!=0) {
		if(*a=='\\') { // escaped character
			a++;
			if (*a != *b) return FALSE;
		}
		else {
			if (*a=='*') {
				if(*(a+1)==0) return TRUE;
				if(*b==0) return FALSE;
				if (strwildcardcmp(a+1, b) || strwildcardcmp(a, b+1)) return TRUE;
				else return FALSE;
			}
			else if (*a!='?' && (*a != *b)) return FALSE;
		}
		a++;
		b++;
	}
	if ((*a == 0 && *b == 0) || (*a=='*' && *(a+1)==0))  return TRUE;
	return FALSE;
}

#define STD_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_ID_FILE

static GInputStream *
get_stream_from_helper (SearchHelper *helper,
                        GFile        *file,
                        GSubprocess **proc,
                        GError      **error)
{
    GSubprocess *helper_proc;
    GSubprocessFlags flags;
    GInputStream *stream;
    GString *command_line;
    gchar **argv;
    gchar *ptr, *path, *quoted;

    path = g_file_get_path (file);
    quoted = g_shell_quote (path);
    g_free (path);

    command_line = g_string_new (helper->exec_format);

    ptr = g_strstr_len (command_line->str, -1, "%s");
    if (ptr != NULL) {
        g_string_erase (command_line, ptr - command_line->str, 2);
        g_string_insert (command_line, ptr - command_line->str, quoted);
    } else {
        g_set_error (error, G_SHELL_ERROR, G_SHELL_ERROR_FAILED,
                     "Search helper exec field missing %%s need to insert file path - '%s'", command_line->str);
        g_string_free (command_line, TRUE);
        g_free (quoted);
        return NULL;
    }

    if (!g_shell_parse_argv (command_line->str,
                             NULL,
                             &argv,
                             error)) {
        g_string_free (command_line, TRUE);
        g_free (quoted);
        return NULL;
    }

    flags = G_SUBPROCESS_FLAGS_STDOUT_PIPE;

    if (!DEBUGGING) {
        flags |= G_SUBPROCESS_FLAGS_STDERR_SILENCE;
    }

    helper_proc = g_subprocess_newv ((const gchar * const *) argv, flags, error);

    stream = NULL;

    if (helper_proc != NULL) {
        stream = g_subprocess_get_stdout_pipe (helper_proc);
        *proc = helper_proc;
    }

    g_strfreev (argv);
    g_free (quoted);
    g_string_free (command_line, TRUE);

    return stream;
}

static gchar *
create_snippet (GMatchInfo  *match_info,
                const gchar *contents,
                gssize       total_length)
{
    gint start_bytes, end_bytes;
    gchar *snippet = NULL;

    if (g_match_info_fetch_pos (match_info, 0, &start_bytes, &end_bytes) && start_bytes >= 0) {
        GString *marked_up;
        glong start, end, new_start, new_end;
        gchar *matched_str, *start_substr, *end_substr, *start_escaped, *end_escaped;

        start = g_utf8_pointer_to_offset (contents, contents + start_bytes);
        end = g_utf8_pointer_to_offset (contents, contents + end_bytes);

        // Extend the snipped forwards and back a bit to give context.
        new_start = MAX (0, start - SNIPPET_EXTEND_SIZE);

        // g_match_info_fetch_pos() can return an end_bytes that == total_length,
        // maybe a utf-8 encoding issue - g_utf8_substring can't deal with this,
        // so just clamp new_end to >= end.
        new_end = MIN (end + SNIPPET_EXTEND_SIZE, MAX (end, total_length - 1));

        matched_str = g_match_info_fetch (match_info, 0);

        start_substr = g_utf8_substring (contents, new_start, start);
        start_escaped = g_markup_escape_text (start_substr, -1);
        g_free (start_substr);

        end_substr = g_utf8_substring (contents, end, new_end);
        end_escaped = g_markup_escape_text (end_substr, -1);
        g_free (end_substr);

        marked_up = g_string_new (NULL);
        g_string_append (marked_up, start_escaped);
        g_string_append (marked_up, "<b>");
        g_string_append (marked_up, matched_str);
        g_string_append (marked_up, "</b>");
        g_string_append (marked_up, end_escaped);

        g_free (start_escaped);
        g_free (end_escaped);
        g_free (matched_str);

        snippet = g_string_free (marked_up, FALSE);
    }

    return snippet;
}

static gchar *
load_contents (SearchThreadData *data,
               GFile            *file,
               SearchHelper     *helper,
               GError          **error)
{
    // TODO: Use flock/mmap for local files?
    GSubprocess *helper_proc;
    GInputStream *stream = NULL;
    GString *str;

    helper_proc = NULL;

    if (helper != NULL) {
        stream = get_stream_from_helper (helper, file, &helper_proc, error);
    } else {
        // text/plain
        stream = G_INPUT_STREAM (g_file_read (file, data->cancellable, error));
    }

    if (stream == NULL) {
        return NULL;
    }

    str = g_string_new (NULL);
    gssize len = 0;

    do {
        gchar chunk[4096];
        len = g_input_stream_read (stream, chunk, 4096, data->cancellable, error);

        if (len <= 0) {
            break;
        }

        if (chunk != NULL) {
            g_string_append_len (str, chunk, len);
        }
    } while (!g_cancellable_is_cancelled (data->cancellable));

    g_input_stream_close (stream,
                          data->cancellable,
                          *error == NULL ? error : NULL);

    // GSubprocess owns the input stream for its STDOUT, but we own it for the text/plain stream.
    if (helper_proc != NULL) {
        g_subprocess_wait (helper_proc,
                           NULL,
                           *error == NULL ? error : NULL);
        g_object_unref (helper_proc);
    } else {
        g_object_unref (stream);
    }

    return g_string_free (str, FALSE);
}

static void
search_for_content_hits (SearchThreadData *data,
                         GFile            *file,
                         SearchHelper     *helper)
{
    GMatchInfo *match_info;
    GError *error;
    gchar *contents = NULL;
    gchar *stripped, *utf8;

    error = NULL;

    contents = load_contents (data, file, helper, &error);

    if (g_cancellable_is_cancelled (data->cancellable)) {
        g_clear_error (&error);
        g_free (contents);
        return;
    }

    if (error != NULL) {
        gchar *uri = g_file_get_uri (file);
        g_warning ("Could not load contents of '%s' during content search: %s", uri, error->message);
        g_free (uri);
        g_error_free (error);
        g_free (contents);
        return;
    }

    utf8 = g_utf8_make_valid (contents, -1);
    g_free (contents);

    if (data->newline_re != NULL) {
        stripped = g_regex_replace_literal (data->newline_re, (const gchar *) utf8, -1, 0, "\n", 0, NULL);
    } else {
        stripped = g_strdup ((const gchar *) utf8);
    }

    g_free (utf8);

    FileSearchResult *fsr = NULL;

    g_regex_match (data->match_re, stripped, 0, &match_info);

    while (g_match_info_matches (match_info) && !g_cancellable_is_cancelled (data->cancellable)) {
        if (fsr == NULL) {
            fsr = file_search_result_new (g_file_get_uri (file));
        }

        file_search_result_add_hit (fsr, create_snippet (match_info, stripped, g_utf8_strlen (stripped, -1)));

        if (!data->count_hits) {
            break;
        }

        if (!g_match_info_next (match_info, &error) && error) {
            g_warning ("Error iterating thru pattern matches (/%s/): code %d - %s",
                       g_regex_get_pattern (data->match_re), error->code, error->message);
            g_error_free (error);
            break;
        }
    }

    g_match_info_unref (match_info);
    g_free (stripped);

    if (fsr != NULL) {
        g_mutex_lock (&data->hit_list_lock);
        data->hit_list = g_list_prepend (data->hit_list, fsr);
        g_mutex_unlock (&data->hit_list_lock);
    }
}

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GFile *child;
	const char *mime_type, *display_name;
	char *cased, *normalized;
	gboolean hit;
	int i;
	const char *id;
	gboolean visited;

    enumerator = g_file_enumerate_children (dir,
                                            STD_ATTRIBUTES "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                            0, data->cancellable, NULL);

	if (enumerator == NULL) {
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL) {
		if (g_file_info_get_is_hidden (info) && !data->show_hidden) {
			goto next;
		}

		display_name = g_file_info_get_display_name (info);
		if (display_name == NULL) {
			goto next;
		}

		normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_NFD);

        if (!data->file_case_sensitive) {
            cased = g_utf8_strdown (normalized, -1);
        } else {
            cased = g_strdup (normalized);
        }

		g_free (normalized);

		hit = data->words_and;
		for (i = 0; data->words[i] != NULL; i++) {
			if (data->word_strstr[i]) {
				if ((strstr (cased, data->words[i]) != NULL)^data->words_and) {
					hit = !data->words_and;
					break;
				}
			}
			else if (strwildcardcmp (data->words[i], cased)^data->words_and) {
				hit = !data->words_and;
				break;
			}
		}
		g_free (cased);

		child = g_file_get_child (dir, g_file_info_get_name (info));
        if (hit) {
            mime_type = g_file_info_get_content_type (info);

            // Our helpers don't currently support uris, so we shouldn't at all -
            // probably best, as search would transfer the contents of every file
            // to our machines.
            if (data->match_re && data->location_supports_content_search) {
                SearchHelper *helper = NULL;

                helper = g_hash_table_lookup (search_helpers, mime_type);

                if (helper != NULL || g_content_type_is_a (mime_type, "text/plain")) {
                    if (DEBUGGING) {
                        g_message ("Evaluating '%s'", g_file_peek_path (child));
                    }
                    search_for_content_hits (data, child, helper);
                }
            } else {
                FileSearchResult *fsr = NULL;

                fsr = file_search_result_new (g_file_get_uri (child));
                g_mutex_lock (&data->hit_list_lock);
                data->hit_list = g_list_prepend (data->hit_list, fsr);
                g_mutex_unlock (&data->hit_list_lock);
            }
        }

		data->n_processed_files++;

        if (data->n_processed_files > (data->match_re ? CONTENT_SEARCH_BATCH_SIZE :
                                                        FILE_SEARCH_ONLY_BATCH_SIZE)) {
            send_batch (data);
        }

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY && data->recurse) {
			id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
			visited = FALSE;
			if (id) {
				if (g_hash_table_lookup_extended (data->visited,
								  id, NULL, NULL)) {
					visited = TRUE;
				} else {
					g_hash_table_insert (data->visited, g_strdup (id), NULL);
				}
			}

			if (!visited) {
				g_queue_push_tail (data->directories, g_object_ref (child));
			}
		}

		g_object_unref (child);
	next:
		g_object_unref (info);
	}

	g_object_unref (enumerator);
}


static gpointer
search_thread_func (gpointer user_data)
{
	SearchThreadData *data;
	GFile *dir;
	GFileInfo *info;
	const char *id;
    gboolean toplevel;
	data = user_data;

	/* Insert id for toplevel directory into visited */
	dir = g_queue_peek_head (data->directories);
	info = g_file_query_info (dir, G_FILE_ATTRIBUTE_ID_FILE, 0, data->cancellable, NULL);
	if (info) {
		id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE);
		if (id) {
			g_hash_table_insert (data->visited, g_strdup (id), NULL);
		}
		g_object_unref (info);
	}

    toplevel = TRUE;

    while (!g_cancellable_is_cancelled (data->cancellable) &&
           (dir = g_queue_pop_head (data->directories)) != NULL) {

        if (!toplevel) {
            if (g_hash_table_contains (data->skip_folders, g_file_peek_path (dir))) {
                g_object_unref (dir);
                continue;
            }

            g_autofree gchar *filename = NULL;
            filename = g_file_get_basename (dir);
            if (g_hash_table_contains (data->skip_folders, filename)) {
                g_object_unref (dir);
                continue;
            }
        }

        toplevel = FALSE;

		visit_directory (dir, data);
		g_object_unref (dir);
	}
	send_batch (data);

	g_idle_add (search_thread_done_idle, data);

	return NULL;
}

static void
nemo_search_engine_advanced_start (NemoSearchEngine *engine)
{
	NemoSearchEngineAdvanced *simple;
	SearchThreadData *data;
	GThread *thread;

	simple = NEMO_SEARCH_ENGINE_ADVANCED (engine);

	if (simple->details->active_search != NULL) {
		return;
	}

	if (simple->details->query == NULL) {
		return;
	}

	data = search_thread_data_new (simple, simple->details->query);

	thread = g_thread_new ("nemo-search-simple", search_thread_func, data);
	simple->details->active_search = data;

	g_thread_unref (thread);
}

static void
nemo_search_engine_advanced_stop (NemoSearchEngine *engine)
{
	NemoSearchEngineAdvanced *simple;

	simple = NEMO_SEARCH_ENGINE_ADVANCED (engine);

	if (simple->details->active_search != NULL) {
		g_cancellable_cancel (simple->details->active_search->cancellable);
		simple->details->active_search = NULL;
	}
}

static void
nemo_search_engine_advanced_set_query (NemoSearchEngine *engine, NemoQuery *query)
{
	NemoSearchEngineAdvanced *simple;

	simple = NEMO_SEARCH_ENGINE_ADVANCED (engine);

	if (query) {
		g_object_ref (query);
	}

	if (simple->details->query) {
		g_object_unref (simple->details->query);
	}

	simple->details->query = query;
}

static void
nemo_search_engine_advanced_class_init (NemoSearchEngineAdvancedClass *class)
{
	GObjectClass *gobject_class;
	NemoSearchEngineClass *engine_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	engine_class = NEMO_SEARCH_ENGINE_CLASS (class);
	engine_class->set_query = nemo_search_engine_advanced_set_query;
	engine_class->start = nemo_search_engine_advanced_start;
	engine_class->stop = nemo_search_engine_advanced_stop;

	g_type_class_add_private (class, sizeof (NemoSearchEngineAdvancedDetails));
}

static void
nemo_search_engine_advanced_init (NemoSearchEngineAdvanced *engine)
{
	engine->details = G_TYPE_INSTANCE_GET_PRIVATE (engine, NEMO_TYPE_SEARCH_ENGINE_ADVANCED,
						       NemoSearchEngineAdvancedDetails);

    if (search_helpers == NULL) {
        initialize_search_helpers (engine);
    }
}

NemoSearchEngine *
nemo_search_engine_advanced_new (void)
{
	NemoSearchEngine *engine;

	engine = g_object_new (NEMO_TYPE_SEARCH_ENGINE_ADVANCED, NULL);

	return engine;
}
