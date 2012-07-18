/*
 * Copyright © 2012 Canonical Ltd.
 *             By Siegfried-A. Gevatter <siegfried.gevatter@collabora.co.uk>
 *
 * License: GPLv2+
 *
 *  The tests in this file require that certain directories and files exist
 *  in order to operate on them. Please use ./run-zeitgeist-test.sh to launch
 *  this test, since it'll create them for you (in a temporary directory).
 *
 * */

#include <glib.h>
#include <glib-object.h>
#include <zeitgeist.h>
#include <string.h>

#include <libnemo-private/nemo-file-operations.h>

// After doing a file operation and before checking Zeitgeist for
// events, we needed to give Nemo some time to insert the events
// (because of g_file_query_info, etc).
#define SLEEP_TIME  400

// Test location
#define TEST_PATH   "/tmp/nemo-zg-test"
#define TEST_URI    "file://" TEST_PATH

typedef struct
{
    ZeitgeistLog        *log;
    GMainLoop           *mainloop;
    ZeitgeistTimeRange  *query_time_range;
    GPtrArray           *expected_events;
    guint               num_expected_events;
} Fixture;

static void
setup_test (Fixture *fix, gconstpointer data)
{
    fix->log = zeitgeist_log_new ();
    fix->mainloop = g_main_loop_new (NULL, FALSE);
    fix->query_time_range = zeitgeist_time_range_new_from_now ();
    fix->expected_events = g_ptr_array_new_with_free_func (
        (GDestroyNotify) g_object_unref);
    fix->num_expected_events = 0;
}

static void teardown (Fixture *fix, gconstpointer data)
{
    g_object_unref (fix->log);
    g_object_unref (fix->query_time_range);
    g_ptr_array_free (fix->expected_events, TRUE);
    g_main_loop_unref (fix->mainloop);

    fix->log = NULL;
    fix->mainloop = NULL;
    fix->query_time_range = NULL;
    fix->expected_events = NULL;
}

static void
test_add_event_assertion (Fixture *fix,
    const char *event_interpretation, const char *uri, const char *current_uri,
    const char *origin, const char *text, const char *mimetype)
{
    // Create subject template
    ZeitgeistSubject *subject_template = zeitgeist_subject_new_full (
        uri, NULL, NULL, mimetype, origin, text, NULL);
    zeitgeist_subject_set_current_uri (subject_template, current_uri);

    // Create event template
    ZeitgeistEvent *event_template = zeitgeist_event_new_full (
        event_interpretation, ZEITGEIST_ZG_USER_ACTIVITY, NULL,
        subject_template, NULL);

    // Add event template to the fixture for later verification
    g_ptr_array_add (fix->expected_events, (gpointer) event_template);
    fix->num_expected_events++;
}

static void
assert_event_matches_template (ZeitgeistEvent *event, ZeitgeistEvent *tmpl)
{
    int i;
    int num_subjects = zeitgeist_event_num_subjects (tmpl);

    g_assert_cmpstr (zeitgeist_event_get_interpretation (event), ==,
        zeitgeist_event_get_interpretation (tmpl));
    g_assert_cmpstr (zeitgeist_event_get_manifestation (event), ==,
        zeitgeist_event_get_manifestation (tmpl));
    g_assert_cmpint (zeitgeist_event_num_subjects (event), ==, num_subjects);

    for (i = 0; i < num_subjects; ++i)
    {
        ZeitgeistSubject *subject = zeitgeist_event_get_subject (event, i);
        ZeitgeistSubject *subj_templ = zeitgeist_event_get_subject (tmpl, i);

        g_assert_cmpstr (zeitgeist_subject_get_uri (subject), ==,
            zeitgeist_subject_get_uri (subj_templ));
        g_assert_cmpstr (zeitgeist_subject_get_current_uri (subject), ==,
            zeitgeist_subject_get_current_uri (subj_templ));
        g_assert_cmpstr (zeitgeist_subject_get_origin (subject), ==,
            zeitgeist_subject_get_origin (subj_templ));
        g_assert_cmpstr (zeitgeist_subject_get_text (subject), ==,
            zeitgeist_subject_get_text (subj_templ));
        g_assert_cmpstr (zeitgeist_subject_get_mimetype (subject), ==,
            zeitgeist_subject_get_mimetype (subj_templ));
    }
}

static void
assert_results_as_expected (Fixture *fix, ZeitgeistResultSet *results)
{
    int i;

    g_assert_cmpint (zeitgeist_result_set_size (results), ==,
        fix->num_expected_events);

    for (i = 0; i < fix->num_expected_events; ++i)
    {
        ZeitgeistEvent *event = zeitgeist_result_set_next (results);
        ZeitgeistEvent *tmpl = g_ptr_array_index (fix->expected_events, i);
        assert_event_matches_template (event, tmpl);
    }
}

static void
_zeitgeist_test_cb2 (ZeitgeistLog *log, GAsyncResult *res, Fixture *fix)
{
    GError *error = NULL;
    ZeitgeistResultSet *results = zeitgeist_log_find_events_finish (
            log, res, &error);
    if (error)
    {
        g_warning ("Error with FindEventIds: %s", error->message);
        g_error_free (error);
        g_assert_not_reached ();
    }

    assert_results_as_expected (fix, results);
    g_object_unref (results);

    g_main_loop_quit (fix->mainloop);
}

static gboolean
_zeitgeist_test_cb1 (Fixture *fix)
{
    zeitgeist_log_find_events (fix->log,
                               fix->query_time_range,
                               g_ptr_array_new (),
                               ZEITGEIST_STORAGE_STATE_ANY,
                               50,
                               ZEITGEIST_RESULT_TYPE_MOST_RECENT_EVENTS,
                               NULL,
                               (GAsyncReadyCallback) _zeitgeist_test_cb2,
                               fix);

    return FALSE;
}

static void
zeitgeist_test_start (GHashTable *debuting_uris, Fixture *fix)
{
    g_timeout_add (SLEEP_TIME, (GSourceFunc) _zeitgeist_test_cb1, (gpointer) fix);
}

static void
zeitgeist_test_start_3 (GHashTable *debuting_uris, gboolean user_cancel,
        gpointer callback_data)
{
    zeitgeist_test_start (debuting_uris, callback_data);
}

static void
test_copy_move (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_MOVE_EVENT,
                                    TEST_URI "/moveme.txt",
                                    TEST_URI "/move_dest/moveme.txt",
                                    TEST_URI "/move_dest",
                                    "moveme.txt",
                                    "text/plain");

    GList *item_uris = NULL;
    item_uris = g_list_prepend (item_uris,
                                TEST_URI "/moveme.txt");

    nemo_file_operations_copy_move (
            item_uris,
            NULL,
            TEST_URI "/move_dest",
            GDK_ACTION_MOVE,
            NULL,
            (NemoCopyCallback) zeitgeist_test_start,
            fix);

    g_main_loop_run (fix->mainloop);
}

static void
test_copy (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_CREATE_EVENT,
                                    TEST_URI "/b.py",
                                    TEST_URI "/b.py",
                                    TEST_URI,
                                    "b.py",
                                    "text/x-python");

    nemo_file_operations_copy_file (
            g_file_new_for_path (TEST_PATH "/a.py"),
            g_file_new_for_path (TEST_PATH),
            "a.py", "b.py",
            NULL,
            (NemoCopyCallback) zeitgeist_test_start,
            fix);

    g_main_loop_run (fix->mainloop);
}

static void
test_new_folder (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_CREATE_EVENT,
                                   TEST_URI "/Untitled%20Folder",
                                   TEST_URI "/Untitled%20Folder",
                                   TEST_URI,
                                   "Untitled Folder",
                                   "inode/directory");

    nemo_file_operations_new_folder (
            NULL, NULL,
            TEST_URI,
            (NemoCopyCallback) zeitgeist_test_start,
            fix);

    g_main_loop_run (fix->mainloop);
}

static void
test_new_file (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_CREATE_EVENT,
                                   TEST_URI "/new_file_test.txt",
                                   TEST_URI "/new_file_test.txt",
                                   TEST_URI,
                                   "new_file_test.txt",
                                   "text/plain");

    const char content[] = "this is the content of a text file...\n";
    nemo_file_operations_new_file (
            NULL, NULL,
            TEST_URI,
            "new_file_test.txt",
            content, strlen (content),
            (NemoCopyCallback) zeitgeist_test_start,
            fix);

    g_main_loop_run (fix->mainloop);
}

static void
test_new_file_from_template (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_CREATE_EVENT,
                                   TEST_URI "/new_tpl_file_test.py",
                                   TEST_URI "/new_tpl_file_test.py",
                                   TEST_URI,
                                   "new_tpl_file_test.py",
                                   "text/x-python");

    nemo_file_operations_new_file_from_template (
            NULL, NULL,
            TEST_URI,
            "new_tpl_file_test.py",
            TEST_URI "/a.py",
            (NemoCopyCallback) zeitgeist_test_start,
            fix);

    g_main_loop_run (fix->mainloop);
}

static void
test_delete (Fixture *fix, gconstpointer data)
{
    test_add_event_assertion (fix, ZEITGEIST_ZG_DELETE_EVENT,
                                   TEST_URI "/del1.txt",
                                   TEST_URI "/del1.txt",
                                   TEST_URI,
                                   "del1.txt",
                                   NULL);
    test_add_event_assertion (fix, ZEITGEIST_ZG_DELETE_EVENT,
                                   TEST_URI "/del2.txt",
                                   TEST_URI "/del2.txt",
                                   TEST_URI,
                                   "del2.txt",
                                   NULL);

    GList *file_list = NULL;
    file_list = g_list_prepend (file_list,
                                g_file_new_for_path (TEST_PATH "/del1.txt"));
    file_list = g_list_prepend (file_list,
                                g_file_new_for_path (TEST_PATH "/del2.txt"));

    // Nemo_file_operations_trash_or_delete is exactly the same
    // NOTE: This operation will ask for manual confirmation of the delete.
    //       There isn't really any nice way around this :(.
    nemo_file_operations_delete (
            file_list,
            NULL,
            (NemoDeleteCallback) zeitgeist_test_start_3,
            fix);

    g_main_loop_run (fix->mainloop);
}

int 
main (int argc, char* argv[])
{
    g_type_init ();

    g_test_init (&argc, &argv, NULL);
    gtk_init (&argc, &argv);

    g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);

    g_test_add ("/Zeitgeist/CopyMove", Fixture, 0,
        setup_test, test_copy_move, teardown);
    g_test_add ("/Zeitgeist/Copy", Fixture, 0,
        setup_test, test_copy, teardown);
    g_test_add ("/Zeitgeist/NewFolder", Fixture, 0,
        setup_test, test_new_folder, teardown);
    g_test_add ("/Zeitgeist/NewFile", Fixture, 0,
        setup_test, test_new_file, teardown);
    g_test_add ("/Zeitgeist/NewFileFromTemplate", Fixture, 0,
        setup_test, test_new_file_from_template, teardown);
    g_test_add ("/Zeitgeist/Delete", Fixture, 0,
        setup_test, test_delete, teardown);

    return g_test_run ();
}
