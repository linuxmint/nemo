/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nemo-bookmark-groups.h"
#include <glib/gstdio.h>

static void
roundtrip (void)
{
    GError *error = NULL;
    char *directory = g_dir_make_tmp ("nemo-groups-XXXXXX", &error);
    g_assert_no_error (error);
    char *filename = g_build_filename (directory, "nemo", "bookmark-groups", NULL);
    GKeyFile *groups = nemo_bookmark_groups_load (filename, &error);
    g_assert_no_error (error);
    char **ids = nemo_bookmark_groups_get_ids (groups);
    g_assert_cmpuint (g_strv_length (ids), ==, 0);
    g_strfreev (ids);
    char *lan = nemo_bookmark_groups_create (groups, "LAN");
    g_assert_nonnull (lan);
    char *work = nemo_bookmark_groups_create (groups, "Arbeit");
    g_assert_nonnull (work);
    g_assert_true (nemo_bookmark_groups_save (groups, filename, &error));
    g_assert_no_error (error);
    g_key_file_unref (groups);
    groups = nemo_bookmark_groups_load (filename, &error);
    g_assert_no_error (error);
    ids = nemo_bookmark_groups_get_ids (groups);
    g_assert_cmpuint (g_strv_length (ids), ==, 2);
    g_assert_cmpstr (ids[0], ==, lan);
    g_assert_cmpstr (ids[1], ==, work);
    char *name = g_key_file_get_string (groups, lan, "Name", NULL);
    g_assert_cmpstr (name, ==, "LAN");
    g_free (name);
    g_strfreev (ids);
    g_key_file_unref (groups);
    g_unlink (filename);
    char *parent = g_path_get_dirname (filename);
    g_rmdir (parent);
    g_rmdir (directory);
    g_free (parent);
    g_free (filename);
    g_free (directory);
    g_free (lan);
    g_free (work);
}
static void
membership (void)
{
    GKeyFile *groups = g_key_file_new ();
    char *lan = nemo_bookmark_groups_create (groups, "LAN");
    char *work = nemo_bookmark_groups_create (groups, "Arbeit");
    nemo_bookmark_groups_move (groups, "file:///mnt/nas", lan, NULL);
    char *id = nemo_bookmark_groups_find (groups, "file:///mnt/nas");
    g_assert_cmpstr (id, ==, lan);
    g_free (id);
    nemo_bookmark_groups_move (groups, "file:///mnt/hermes", lan, "file:///mnt/nas");
    char **uris = g_key_file_get_string_list (groups, lan, "URIs", NULL, NULL);
    g_assert_cmpuint (g_strv_length (uris), ==, 2);
    g_assert_cmpstr (uris[0], ==, "file:///mnt/hermes");
    g_strfreev (uris);
    nemo_bookmark_groups_move (groups, "file:///mnt/nas", work, NULL);
    id = nemo_bookmark_groups_find (groups, "file:///mnt/nas");
    g_assert_cmpstr (id, ==, work);
    g_free (id);
    nemo_bookmark_groups_move (groups, "file:///mnt/nas", NULL, NULL);
    g_assert_null (nemo_bookmark_groups_find (groups, "file:///mnt/nas"));
    g_free (lan);
    g_free (work);
    g_key_file_unref (groups);
}
static void
lifecycle (void)
{
    GKeyFile *groups = g_key_file_new ();
    char *lan = nemo_bookmark_groups_create (groups, "LAN");
    nemo_bookmark_groups_move (groups, "file:///mnt/nas", lan, NULL);
    nemo_bookmark_groups_delete (groups, lan);
    char **ids = nemo_bookmark_groups_get_ids (groups);
    g_assert_cmpuint (g_strv_length (ids), ==, 0);
    g_assert_null (nemo_bookmark_groups_find (groups, "file:///mnt/nas"));
    g_strfreev (ids);
    g_free (lan);
    g_key_file_unref (groups);
}
static void
rename_group (void)
{
    GKeyFile *groups = g_key_file_new ();
    char *id = nemo_bookmark_groups_create (groups, "LAN");
    g_assert_true (nemo_bookmark_groups_rename (groups, id, "Netzwerk"));
    char *name = g_key_file_get_string (groups, id, "Name", NULL);
    g_assert_cmpstr (name, ==, "Netzwerk");
    g_free (name);
    g_free (id);
    g_key_file_unref (groups);
}
static void
prune (void)
{
    GKeyFile *groups = g_key_file_new ();
    char *id = nemo_bookmark_groups_create (groups, "LAN");
    nemo_bookmark_groups_move (groups, "file:///keep", id, NULL);
    nemo_bookmark_groups_move (groups, "file:///deleted", id, NULL);
    const char *uris[] = { "file:///keep", NULL };
    g_assert_true (nemo_bookmark_groups_prune (groups, uris));
    g_assert_false (nemo_bookmark_groups_prune (groups, uris));
    g_assert_null (nemo_bookmark_groups_find (groups, "file:///deleted"));
    char *found = nemo_bookmark_groups_find (groups, "file:///keep");
    g_assert_cmpstr (found, ==, id);
    g_free (found);
    g_free (id);
    g_key_file_unref (groups);
}
static void
move_before_self (void)
{
    GKeyFile *groups = g_key_file_new ();
    char *id = nemo_bookmark_groups_create (groups, "LAN");
    nemo_bookmark_groups_move (groups, "file:///first", id, NULL);
    nemo_bookmark_groups_move (groups, "file:///second", id, NULL);
    nemo_bookmark_groups_move (groups, "file:///first", id, "file:///first");
    char **uris = g_key_file_get_string_list (groups, id, "URIs", NULL, NULL);
    g_assert_cmpstr (uris[0], ==, "file:///first");
    g_assert_cmpstr (uris[1], ==, "file:///second");
    g_strfreev (uris);
    g_free (id);
    g_key_file_unref (groups);
}
int main (int argc, char **argv) {
    g_test_init (&argc, &argv, NULL);
    g_test_add_func ("/bookmark-groups/roundtrip", roundtrip);
    g_test_add_func ("/bookmark-groups/membership", membership);
    g_test_add_func ("/bookmark-groups/delete", lifecycle);
    g_test_add_func ("/bookmark-groups/rename", rename_group);
    g_test_add_func ("/bookmark-groups/prune", prune);
    g_test_add_func ("/bookmark-groups/move-before-self", move_before_self);
    return g_test_run ();
}
