#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-search-engine-advanced.h>

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>

typedef struct {
    GMainLoop *loop;
    guint hits;
    gchar *snippet;
    gboolean timed_out;
} SearchResult;

static void
hits_added_cb (NemoSearchEngine *engine,
               GList            *hits,
               SearchResult     *result)
{
    for (GList *l = hits; l != NULL; l = l->next) {
        FileSearchResult *hit = l->data;

        result->hits++;
        if (result->snippet == NULL) {
            result->snippet = g_strdup (hit->snippet);
        }
        file_search_result_free (hit);
    }
}

static void
finished_cb (NemoSearchEngine *engine,
             SearchResult     *result)
{
    g_main_loop_quit (result->loop);
}

static gboolean
timeout_cb (gpointer user_data)
{
    SearchResult *result = user_data;

    result->timed_out = TRUE;
    g_main_loop_quit (result->loop);
    return G_SOURCE_REMOVE;
}

static void
test_content_search_matches_precomposed_unicode (void)
{
    g_autofree gchar *tmpdir = NULL;
    g_autofree gchar *path = NULL;
    g_autofree gchar *uri = NULL;
    NemoQuery *query = NULL;
    NemoSearchEngine *engine = NULL;
    g_autoptr (GError) error = NULL;
    SearchResult result = { 0 };
    guint timeout_id;

    tmpdir = g_dir_make_tmp ("nemo-search-content-XXXXXX", &error);
    g_assert_no_error (error);

    path = g_build_filename (tmpdir, "katakana.txt", NULL);
    g_file_set_contents (path, "ランプ\n", -1, &error);
    g_assert_no_error (error);

    uri = g_filename_to_uri (tmpdir, NULL, &error);
    g_assert_no_error (error);

    query = nemo_query_new ();
    nemo_query_set_file_pattern (query, "*");
    nemo_query_set_content_pattern (query, "ランプ");
    nemo_query_set_location (query, uri);

    engine = nemo_search_engine_advanced_new ();
    result.loop = g_main_loop_new (NULL, FALSE);

    g_signal_connect (engine, "hits-added", G_CALLBACK (hits_added_cb), &result);
    g_signal_connect (engine, "finished", G_CALLBACK (finished_cb), &result);

    nemo_search_engine_set_query (engine, query);
    nemo_search_engine_start (engine);

    timeout_id = g_timeout_add_seconds (5, timeout_cb, &result);
    g_main_loop_run (result.loop);
    if (!result.timed_out) {
        g_source_remove (timeout_id);
    }

    g_assert_false (result.timed_out);
    g_assert_cmpuint (result.hits, ==, 1);
    g_assert_nonnull (result.snippet);
    g_assert_nonnull (g_strstr_len (result.snippet, -1, "ランプ"));

    g_free (result.snippet);
    g_main_loop_unref (result.loop);
    g_object_unref (engine);
    g_object_unref (query);
    g_remove (path);
    g_rmdir (tmpdir);
}

int
main (int argc, char **argv)
{
    g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
    g_test_init (&argc, &argv, NULL);
    nemo_global_preferences_init ();

    g_test_add_func ("/search/content/precomposed-unicode", test_content_search_matches_precomposed_unicode);

    return g_test_run ();
}
