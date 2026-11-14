#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <libnemo-private/nemo-progress-info.h>

/* The size is read by a worker thread: poll until it shows up. */
static gint64
wait_live (NemoProgressInfo *info, gint64 expected)
{
    gint64 v = -1;
    guint i;

    for (i = 0; i < 400; i++) {
        v = nemo_progress_info_get_live_bytes (info);
        if (v == expected) {
            break;
        }
        g_usleep (5000);
    }
    return v;
}

/* Give the worker time to stat, then check there is still no size. */
static void
assert_none (NemoProgressInfo *info)
{
    guint i;

    for (i = 0; i < 12; i++) {
        g_assert_cmpint (nemo_progress_info_get_live_bytes (info), ==, -1);
        g_usleep (5000);
    }
}

static void
test_live_bytes (void)
{
    NemoProgressInfo *info = nemo_progress_info_new ();
    g_autofree gchar *dir = g_dir_make_tmp ("nemo-live-XXXXXX", NULL);
    g_autofree gchar *path = g_build_filename (dir, "f", NULL);
    g_autofree gchar *link = NULL;

    /* nothing set */
    g_assert_cmpint (nemo_progress_info_get_live_bytes (info), ==, -1);

    /* file does not exist yet */
    nemo_progress_info_set_live_file (info, path, 1000);
    assert_none (info);

    /* grows while being written */
    g_file_set_contents (path, "0123456789", 10, NULL);
    g_assert_cmpint (wait_live (info, 1010), ==, 1010);
    g_file_set_contents (path, "0123456789abcdef", 16, NULL);
    g_assert_cmpint (wait_live (info, 1016), ==, 1016);

    /* a directory is not a file being written */
    nemo_progress_info_set_live_file (info, dir, 5);
    assert_none (info);

    /* nor is a symlink, even one pointing at a real file */
    link = g_build_filename (dir, "l", NULL);
    g_assert_cmpint (symlink (path, link), ==, 0);
    nemo_progress_info_set_live_file (info, link, 5);
    assert_none (info);
    g_unlink (link);

    /* a new file starts from its own size, not the previous one's */
    nemo_progress_info_set_live_file (info, path, 2000);
    g_assert_cmpint (wait_live (info, 2016), ==, 2016);

    /* cleared again */
    nemo_progress_info_set_live_file (info, path, 0);
    nemo_progress_info_set_live_file (info, NULL, 0);
    g_assert_cmpint (nemo_progress_info_get_live_bytes (info), ==, -1);

    g_unlink (path);
    g_rmdir (dir);
    g_object_unref (info);
}

static void
test_smooth_progress (void)
{
    NemoProgressInfo *info = nemo_progress_info_new ();
    g_autofree gchar *dir = g_dir_make_tmp ("nemo-smooth-XXXXXX", NULL);
    g_autofree gchar *path = g_build_filename (dir, "f", NULL);
    double p = 0;
    guint i;

    /* no live file: just what was reported */
    nemo_progress_info_set_progress (info, 100, 1000);
    g_assert_cmpfloat_with_epsilon (nemo_progress_info_get_smooth_progress (info), 0.1, 1e-9);

    /* the destination grows beyond the last report: the bar follows */
    g_file_set_contents (path, "0123456789", 10, NULL);
    nemo_progress_info_set_live_file (info, path, 400);
    for (i = 0; i < 400; i++) {
        p = nemo_progress_info_get_smooth_progress (info);
        if (p > 0.40) {
            break;
        }
        g_usleep (5000);
    }
    g_assert_cmpfloat_with_epsilon (p, 0.41, 1e-9);

    /* never goes below the reported value, never above 1 */
    nemo_progress_info_set_progress (info, 900, 1000);
    g_assert_cmpfloat (nemo_progress_info_get_smooth_progress (info), >=, 0.9);
    g_file_set_contents (path, "0123456789", 10, NULL);
    nemo_progress_info_set_live_file (info, path, 5000);
    for (i = 0; i < 400; i++) {
        p = nemo_progress_info_get_smooth_progress (info);
        if (p >= 1.0) {
            break;
        }
        g_usleep (5000);
    }
    g_assert_cmpfloat (p, ==, 1.0);

    /* cleared again: back to the reported value */
    nemo_progress_info_set_live_file (info, NULL, 0);
    g_assert_cmpfloat_with_epsilon (nemo_progress_info_get_smooth_progress (info), 0.9, 1e-9);

    g_unlink (path);
    g_rmdir (dir);
    g_object_unref (info);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/progress-info/live-bytes", test_live_bytes);
    g_test_add_func ("/progress-info/smooth-progress", test_smooth_progress);
    return g_test_run ();
}
