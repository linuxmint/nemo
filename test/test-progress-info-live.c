#include <unistd.h>
#include <fcntl.h>
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

/* Overwrite: GLib writes ".goutputstream-*" in the target directory. It is
 * found among our open descriptors, and two jobs never share one. */
static void
test_live_temp (void)
{
    NemoProgressInfo *a = nemo_progress_info_new ();
    NemoProgressInfo *b = nemo_progress_info_new ();
    g_autofree gchar *dir = g_dir_make_tmp ("nemo-temp-XXXXXX", NULL);
    g_autofree gchar *t1 = g_build_filename (dir, ".goutputstream-AAAAAA", NULL);
    g_autofree gchar *t2 = g_build_filename (dir, ".goutputstream-BBBBBB", NULL);
    g_autofree gchar *other = g_build_filename (dir, "regular", NULL);
    int fd1, fd2, fd3;

    fd3 = g_open (other, O_CREAT | O_WRONLY, 0600);
    nemo_progress_info_set_live_temp (a, dir, 100);
    /* nothing open yet, and a regular file is not a temp file */
    assert_none (a);

    fd1 = g_open (t1, O_CREAT | O_WRONLY, 0600);
    g_assert_cmpint (write (fd1, "0123456789", 10), ==, 10);
    g_assert_cmpint (wait_live (a, 110), ==, 110);

    /* keeps following it as it grows */
    g_assert_cmpint (write (fd1, "0123456789", 10), ==, 10);
    g_assert_cmpint (wait_live (a, 120), ==, 120);

    /* a second job in the same directory gets the other file, not this one */
    nemo_progress_info_set_live_temp (b, dir, 0);
    assert_none (b);
    fd2 = g_open (t2, O_CREAT | O_WRONLY, 0600);
    g_assert_cmpint (write (fd2, "012345", 6), ==, 6);
    g_assert_cmpint (wait_live (b, 6), ==, 6);
    g_assert_cmpint (wait_live (a, 120), ==, 120);

    /* renamed over the target at the end: the file is gone, look again */
    g_assert_cmpint (g_unlink (t1), ==, 0);
    close (fd1);
    /* the cached size goes away once the worker has looked again */
    g_assert_cmpint (wait_live (a, -1), ==, -1);
    assert_none (a);

    /* clearing releases the claim for the next file */
    nemo_progress_info_set_live_file (b, NULL, 0);
    nemo_progress_info_set_live_temp (a, dir, 0);
    g_assert_cmpint (wait_live (a, 6), ==, 6);

    close (fd2);
    close (fd3);
    g_unlink (t2);
    g_unlink (other);
    g_rmdir (dir);
    g_object_unref (a);
    g_object_unref (b);
}

int
main (int argc, char **argv)
{
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/progress-info/live-bytes", test_live_bytes);
    g_test_add_func ("/progress-info/live-temp", test_live_temp);
    g_test_add_func ("/progress-info/smooth-progress", test_smooth_progress);
    return g_test_run ();
}
