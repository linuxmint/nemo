/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "nemo-bookmark-groups.h"
#include <glib/gstdio.h>
#include <errno.h>

char *
nemo_bookmark_groups_filename (void)
{
    return g_build_filename (g_get_user_config_dir (), "nemo", "bookmark-groups", NULL);
}

GKeyFile *
nemo_bookmark_groups_load (const char *filename, GError **error)
{
    GKeyFile *groups = g_key_file_new ();
    GError *local_error = NULL;
    if (!g_key_file_load_from_file (groups, filename, G_KEY_FILE_NONE, &local_error)) {
        if (!g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            g_propagate_error (error, local_error);
            g_key_file_unref (groups);
            return NULL;
        }
        g_clear_error (&local_error);
    }
    return groups;
}

gboolean
nemo_bookmark_groups_save (GKeyFile *groups, const char *filename, GError **error)
{
    char *directory = g_path_get_dirname (filename);
    if (g_mkdir_with_parents (directory, 0700) < 0) {
        g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno),
                     "Could not create %s: %s", directory, g_strerror (errno));
        g_free (directory);
        return FALSE;
    }
    g_free (directory);
    return g_key_file_save_to_file (groups, filename, error);
}

char **
nemo_bookmark_groups_get_ids (GKeyFile *groups)
{
    char **ids = g_key_file_get_string_list (groups, "Groups", "Order", NULL, NULL);
    return ids != NULL ? ids : g_new0 (char *, 1);
}

char *
nemo_bookmark_groups_create (GKeyFile *groups, const char *name)
{
    char **ids = nemo_bookmark_groups_get_ids (groups);
    guint length = g_strv_length (ids);
    char *id = g_uuid_string_random ();
    ids = g_realloc_n (ids, length + 2, sizeof (char *));
    ids[length] = g_strdup (id);
    ids[length + 1] = NULL;
    g_key_file_set_string_list (groups, "Groups", "Order", (const char * const *) ids, length + 1);
    g_key_file_set_string (groups, id, "Name", name);
    g_key_file_set_boolean (groups, id, "Collapsed", FALSE);
    g_strfreev (ids);
    return id;
}

gboolean
nemo_bookmark_groups_rename (GKeyFile *groups, const char *id, const char *name)
{
    if (!g_key_file_has_group (groups, id) || name == NULL || *name == '\0')
        return FALSE;
    g_key_file_set_string (groups, id, "Name", name);
    return TRUE;
}
void
nemo_bookmark_groups_delete (GKeyFile *groups, const char *id)
{
    char **ids = nemo_bookmark_groups_get_ids (groups);
    GPtrArray *remaining = g_ptr_array_new ();
    for (guint i = 0; ids[i] != NULL; i++) {
        if (g_strcmp0 (ids[i], id) != 0)
            g_ptr_array_add (remaining, ids[i]);
    }
    g_key_file_set_string_list (groups, "Groups", "Order",
                                (const char * const *) remaining->pdata, remaining->len);
    g_key_file_remove_group (groups, id, NULL);
    g_ptr_array_unref (remaining);
    g_strfreev (ids);
}
char *
nemo_bookmark_groups_find (GKeyFile *groups, const char *uri)
{
    char **ids = nemo_bookmark_groups_get_ids (groups);
    char *result = NULL;
    for (guint i = 0; ids[i] != NULL; i++) {
        char **uris = g_key_file_get_string_list (groups, ids[i], "URIs", NULL, NULL);
        if (uris != NULL && g_strv_contains ((const char * const *) uris, uri)) {
            result = g_strdup (ids[i]);
            g_strfreev (uris);
            break;
        }
        g_strfreev (uris);
    }
    g_strfreev (ids);
    return result;
}

void
nemo_bookmark_groups_move (GKeyFile *groups, const char *uri, const char *id, const char *before_uri)
{
    if (g_strcmp0 (uri, before_uri) == 0) {
        char *current_id = nemo_bookmark_groups_find (groups, uri);
        gboolean same_group = g_strcmp0 (current_id, id) == 0;
        g_free (current_id);
        if (same_group)
            return;
    }
    char **ids = nemo_bookmark_groups_get_ids (groups);
    /* Never lose a membership because a stale menu targets a deleted group. */
    if (id != NULL && !g_strv_contains ((const char * const *) ids, id)) {
        g_strfreev (ids);
        return;
    }
    for (guint i = 0; ids[i] != NULL; i++) {
        char **uris = g_key_file_get_string_list (groups, ids[i], "URIs", NULL, NULL);
        GPtrArray *ordered = g_ptr_array_new ();
        gboolean inserted = FALSE;
        for (guint j = 0; uris != NULL && uris[j] != NULL; j++) {
            if (g_strcmp0 (uris[j], uri) == 0)
                continue;
            if (g_strcmp0 (ids[i], id) == 0 && g_strcmp0 (uris[j], before_uri) == 0) {
                g_ptr_array_add (ordered, (gpointer) uri);
                inserted = TRUE;
            }
            g_ptr_array_add (ordered, uris[j]);
        }
        if (g_strcmp0 (ids[i], id) == 0 && !inserted)
            g_ptr_array_add (ordered, (gpointer) uri);
        g_key_file_set_string_list (groups, ids[i], "URIs",
                                    (const char * const *) ordered->pdata, ordered->len);
        g_ptr_array_unref (ordered);
        g_strfreev (uris);
    }
    g_strfreev (ids);
}
gboolean
nemo_bookmark_groups_prune (GKeyFile *groups, const char * const *uris)
{
    char **ids = nemo_bookmark_groups_get_ids (groups);
    gboolean changed = FALSE;
    for (guint i = 0; ids[i] != NULL; i++) {
        char **assigned = g_key_file_get_string_list (groups, ids[i], "URIs", NULL, NULL);
        GPtrArray *remaining = g_ptr_array_new ();
        guint length = assigned != NULL ? g_strv_length (assigned) : 0;
        for (guint j = 0; j < length; j++) {
            if (g_strv_contains (uris, assigned[j]))
                g_ptr_array_add (remaining, assigned[j]);
        }
        if (remaining->len != length) {
            g_key_file_set_string_list (groups, ids[i], "URIs",
                                        (const char * const *) remaining->pdata, remaining->len);
            changed = TRUE;
        }
        g_ptr_array_unref (remaining);
        g_strfreev (assigned);
    }
    g_strfreev (ids);
    return changed;
}
