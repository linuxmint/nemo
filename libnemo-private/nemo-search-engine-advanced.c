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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gio/gio.h>

#define DEBUG_FLAG NEMO_DEBUG_SEARCH
#include "nemo-debug.h"

#ifndef GLIB_VERSION_2_70
#define g_pattern_spec_match g_pattern_match
#endif

#define SEARCH_HELPER_GROUP "Nemo Search Helper"

#define FILE_SEARCH_ONLY_BATCH_SIZE 500
#define CONTENT_SEARCH_BATCH_SIZE 1
#define SNIPPET_EXTEND_SIZE 100

typedef struct {
    gchar *def_path;
    gchar *exec_format;

    gint priority;
    /* future? */
} SearchHelper;

typedef struct {
	NemoSearchEngineAdvanced *engine;
	GCancellable *cancellable;

	GList *mime_types;

	GQueue *directories; /* GFiles */

	GHashTable *visited;
    GHashTable *skip_folders;

	gint n_processed_files;
    GRegex *content_re;
    GRegex *newline_re;

    GRegex *filename_re;
    GPatternSpec *filename_glob_pattern;

    GMutex hit_list_lock;
    GList *hit_list; // holds FileSearchResults

    gboolean show_hidden;
    gboolean count_hits;
    gboolean recurse;
    gboolean file_case_sensitive;
    gboolean file_use_regex;
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
    g_free (helper->def_path);
    g_free (helper->exec_format);
    g_free (helper);
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
            DEBUG ("Existing nemo search_helper for '%s' (%s) has higher priority than a new one (%s), ignoring the new one.",
                   mime_type, existing->def_path, path);
            continue;
        } else if (existing) {
            DEBUG ("Replacing existing nemo search_helper for '%s' (%s) with %s based on priority.",
                   mime_type, existing->def_path, path);
        }

        helper = g_new0 (SearchHelper, 1);
        helper->def_path = g_strdup (path);
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

static GRegex *
nemo_search_engine_advanced_create_filename_regex (NemoQuery  *query,
                                                   GError    **error)
{
    GRegexCompileFlags flags;
    g_autofree gchar *text = NULL;
    g_autofree gchar *normalized = NULL;
    g_autofree gchar *format = NULL;

    if (!nemo_query_get_use_file_regex (query)) {
        // No regex, but no error, only needed for nemo_search_engine_advanced_check_filename_pattern()
        return NULL;
    }

    text = nemo_query_get_file_pattern (query);
    normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);

    flags = G_REGEX_OPTIMIZE;

    format = g_settings_get_string (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_REGEX_FORMAT);
    if (g_strcmp0 (format, "javascript") == 0) {
        flags |= G_REGEX_JAVASCRIPT_COMPAT;
    }

    if (g_settings_get_boolean (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_USE_RAW)) {
        flags |= G_REGEX_RAW;
    }

    if (!nemo_query_get_file_case_sensitive (query)) {
        flags |= G_REGEX_CASELESS;
    }

    return g_regex_new (normalized,
                        flags,
                        0,
                        error);
}

static GRegex *
nemo_search_engine_advanced_create_content_regex (NemoQuery  *query,
                                                  GError    **error)
{
    GRegexCompileFlags flags;
    g_autofree gchar *text = NULL;
    g_autofree gchar *normalized = NULL;
    g_autofree gchar *escaped = NULL;
    g_autofree gchar *format = NULL;

    text = nemo_query_get_content_pattern (query);
    normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);

    if (nemo_query_get_use_content_regex (query)) {
        escaped = g_strdup (normalized);
    } else {
        escaped = g_regex_escape_string (normalized, -1);
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

    return g_regex_new (escaped,
                        flags,
                        0,
                        error);
}

static SearchThreadData *
search_thread_data_new (NemoSearchEngineAdvanced *engine,
			NemoQuery *query)
{
    GError *error = NULL;
    SearchThreadData *data;
    char *uri;
    GFile *location;
    gint i;

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

    data->file_case_sensitive = nemo_query_get_file_case_sensitive (query);
    data->file_use_regex = nemo_query_get_use_file_regex (query);

    if (data->file_use_regex) {
        data->filename_re = nemo_search_engine_advanced_create_filename_regex (query, &error);

        if (data->filename_re == NULL) {
            if (error != NULL) {
                g_warning ("Filename pattern is invalid: code %d - %s", error->code, error->message);
            }
            g_clear_error (&error);
        } else {
            DEBUG ("regex is '%s'", g_regex_get_pattern (data->filename_re));
        }
    } else {
        gchar *text, *normalized, *cased;

        text = nemo_query_get_file_pattern (query);
        normalized = g_utf8_normalize (text, -1, G_NORMALIZE_NFD);

        if (!data->file_case_sensitive) {
            cased = g_utf8_strdown (normalized, -1);
        } else {
            cased = g_strdup (normalized);
        }

        data->filename_glob_pattern = g_pattern_spec_new (cased);

        g_free (text);
        g_free (normalized);
        g_free (cased);
    }

    data->skip_folders = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
    gchar **folders_array = g_settings_get_strv (nemo_search_preferences, NEMO_PREFERENCES_SEARCH_SKIP_FOLDERS);
    for (i = 0; i < g_strv_length (folders_array); i++) {
        /* Don't add an ancestor of the current location if it's in the skip list */
        if (g_str_has_prefix (g_file_peek_path (location), folders_array[i])) {
            DEBUG ("Ignoring skip folder that is an ancestor to the search root: '%s'", folders_array[i]);
            continue;
        }

        DEBUG ("Skipping folder in search: '%s'", folders_array[i]);
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

    if (nemo_query_has_content_pattern (query)) {
        data->content_re = nemo_search_engine_advanced_create_content_regex (query, &error);

        if (data->content_re == NULL) {
            if (error != NULL) {
                // TODO: Maybe do something in the ui, make the info bar red?
                g_warning ("Content pattern is invalid: code %d - %s", error->code, error->message);
            }
            g_clear_error (&error);
        } else {
            DEBUG ("regex is '%s'", g_regex_get_pattern (data->content_re));
        }

        data->newline_re = g_regex_new ("[\\n\\r]{2,}",
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
	g_list_free_full (data->mime_types, g_free);
	g_list_free_full (data->hit_list, (GDestroyNotify) file_search_result_free);
    g_clear_pointer (&data->content_re, g_regex_unref);
    g_clear_pointer (&data->newline_re, g_regex_unref);
    g_clear_pointer (&data->filename_re, g_regex_unref);
    g_clear_pointer (&data->filename_glob_pattern, g_pattern_spec_free);
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
        g_list_free (hits->hit_list);
    } else {
        // FileSearchResults are normally freed in NemoSearchDirectory reset_file_list()
        g_list_free_full (hits->hit_list, (GDestroyNotify) file_search_result_free);
    }

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

#define STD_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
	G_FILE_ATTRIBUTE_ID_FILE

#define CONTENT_SEARCH_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE

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
                     "Search helper exec field missing %%s needed to insert file path - '%s'", command_line->str);
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

    g_regex_match (data->content_re, stripped, 0, &match_info);

    while (g_match_info_matches (match_info) && !g_cancellable_is_cancelled (data->cancellable)) {
        if (fsr == NULL) {
            fsr = file_search_result_new (g_file_get_uri (file), create_snippet (match_info, stripped, g_utf8_strlen (stripped, -1)));
        }

        if (!data->count_hits) {
            break;
        }

        file_search_result_add_hit (fsr);

        if (!g_match_info_next (match_info, &error) && error) {
            g_warning ("Error iterating thru pattern matches (/%s/): code %d - %s",
                       g_regex_get_pattern (data->content_re), error->code, error->message);
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

static gboolean
hash_func_check_skip_file (gpointer key,
                           gpointer value,
                           gpointer user_data)
{
    const gchar *entry = key;
    const gchar *path = user_data;

    /* Check the absolute path prefix for skip entries like '/proc' */
    if (g_str_has_prefix (path, entry)) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
hash_func_check_skip_dir (gpointer key,
                          gpointer value,
                          gpointer user_data)
{
    const gchar *entry = key;
    const gchar *path = user_data;

    /* Check the absolute path prefix for skip entries like '/proc' */
    if (g_str_has_prefix (path, entry)) {
        return TRUE;
    }

    /* Check the basename for non-absolute file/folder names */
    g_autofree gchar *basename = g_path_get_basename (path);
    if (g_strcmp0 (entry, basename) == 0) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
should_skip_child (SearchThreadData *data, GFileInfo *info, GFile *file, gboolean is_dir)
{
    const gchar *path = g_file_peek_path (file);
    g_autofree gchar *resolved_path = realpath (path, NULL);

    DEBUG ("Skip check: '%s' realpath is '%s'", path, resolved_path);

    if (resolved_path != NULL) {
        GHRFunc func = is_dir ? hash_func_check_skip_dir : hash_func_check_skip_file;
        if (!g_hash_table_find (data->skip_folders, func, resolved_path)) {
            return FALSE;
        }
    }

    DEBUG ("Skip check: skipping '%s' because realpath is invalid or skipped", path);

    return TRUE;
}

static gboolean
find_wildcard_mime_type (gpointer key, gpointer value, gpointer user_data)
{
    const gchar *helper_mime_type = key;
    const gchar *file_content_type = user_data;

    return g_content_type_is_mime_type (file_content_type, helper_mime_type);
}

static void
visit_directory (GFile *dir, SearchThreadData *data)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
    GFile *child;
	const char *display_name;
	char *normalized;
	gboolean hit, is_dir, skip_child;

    const gchar *attrs;

    if (data->content_re)
        attrs = STD_ATTRIBUTES "," CONTENT_SEARCH_ATTRIBUTES;
    else
        attrs = STD_ATTRIBUTES;

    enumerator = g_file_enumerate_children (dir,
                                            attrs,
                                            0, data->cancellable, NULL);

	if (enumerator == NULL) {
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, data->cancellable, NULL)) != NULL) {
		if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) && !data->show_hidden) {
			goto next;
		}

		display_name = g_file_info_get_display_name (info);
		if (display_name == NULL) {
			goto next;
		}

		normalized = g_utf8_normalize (display_name, -1, G_NORMALIZE_NFD);

        if (data->file_use_regex) {
            GMatchInfo *match_info;
            hit = g_regex_match (data->filename_re, normalized, 0, &match_info);
            g_match_info_unref (match_info);
        } else {
            gchar *cased;

            if (!data->file_case_sensitive) {
                cased = g_utf8_strdown (normalized, -1);
            } else {
                cased = g_strdup (normalized);
            }

            gchar *cased_reversed = g_utf8_strreverse (cased, -1);
            hit = g_pattern_spec_match (data->filename_glob_pattern, strlen (cased), cased, cased_reversed);
            g_free (cased);
            g_free (cased_reversed);
        }

        g_free (normalized);

        child = g_file_get_child (dir, g_file_info_get_name (info));
        is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;

        /* Long explanation, to preserve intent in the future ...
         *
         * For normal files, links can appear in a simple filename search, so we allow it as a
         * 'hit'. But we avoid following the link if its realpath takes us into a skipped folder
         * (unless it's an ancestor of our starting folder - determined in search_thread_data_new ()).
         * For directories, we check base names in addition to the realpath check, and don't
         * queue them.

         * Example:
         *  test-folder:
         *     - aaa
         *     - bbb
         *     - ccc
         *     - cccdir/aaachild
         *     - aaadir
         *     - aaadirlink -> /run/usr/1000
         *     - aaalink -> /proc/self/pagemap
         *
         * Starting the search in test-folder:
         * - recursive filename search for 'aaa' returns 'aaa', 'aaadir', 'aaadirlink', 'aaalink', 'aaachild'
         * - content search * files for 'aaa': searches 'aaa', 'bbb', 'ccc', 'aaachild'
         *
         * Adding 'ccc' to the skip list:
         * - recursive filename search no longer finds 'aaachild'
         * Entering 'ccc':
         * - recursive filename search for 'aaa' finds 'aaachild' (a skip entry is ignored if we're in that directory or one of its descendants.)
         */
        skip_child = should_skip_child (data, info, child, is_dir);

        if (hit) {
            const gchar *content_type;

            content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

            if (content_type == NULL) {
                content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
            }

            g_autofree gchar *mime_type = g_content_type_get_mime_type (content_type);

            // Our helpers don't currently support uris, so we shouldn't at all -
            // probably best, as search would transfer the contents of every file
            // to our machines.
            if (data->content_re && data->location_supports_content_search) {
                if (!skip_child) {
                    SearchHelper *helper = NULL;

                    helper = g_hash_table_lookup (search_helpers, mime_type);
                    if (helper == NULL) {
                        helper = g_hash_table_find (search_helpers, find_wildcard_mime_type, (gpointer) content_type);
                    }

                    if (helper != NULL || g_content_type_is_a (content_type, "text/plain")) {
                        if (DEBUGGING) {
                            g_message ("Evaluating '%s'", g_file_peek_path (child));
                        }
                        search_for_content_hits (data, child, helper);
                    }
                }
            } else {
                FileSearchResult *fsr = NULL;

                fsr = file_search_result_new (g_file_get_uri (child), NULL);
                g_mutex_lock (&data->hit_list_lock);
                data->hit_list = g_list_prepend (data->hit_list, fsr);
                g_mutex_unlock (&data->hit_list_lock);
            }
        }

		data->n_processed_files++;

        if (data->n_processed_files > (data->content_re ? CONTENT_SEARCH_BATCH_SIZE :
                                                        FILE_SEARCH_ONLY_BATCH_SIZE)) {
            send_batch (data);
        }

		if (is_dir && data->recurse && !skip_child) {
            gboolean visited;
            const char *id;

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

    while (!g_cancellable_is_cancelled (data->cancellable) &&
           (dir = g_queue_pop_head (data->directories)) != NULL) {

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

gboolean
nemo_search_engine_advanced_check_filename_pattern (NemoQuery   *query,
                                                    GError     **error)
{
    GRegex *regex;
    gboolean ret = FALSE;

    regex = nemo_search_engine_advanced_create_filename_regex (query, error);

    if (regex != NULL) {
        ret = TRUE;
    }
    g_clear_pointer (&regex, g_regex_unref);
    return ret;
}

gboolean
nemo_search_engine_advanced_check_content_pattern (NemoQuery *query,
                                                   GError   **error)
{
    GRegex *regex;
    gboolean ret = FALSE;

    regex = nemo_search_engine_advanced_create_content_regex (query, error);

    if (regex != NULL) {
        ret = TRUE;
    }

    g_clear_pointer (&regex, g_regex_unref);
    return ret;
}
