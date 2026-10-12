/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef NEMO_BOOKMARK_GROUPS_H
#define NEMO_BOOKMARK_GROUPS_H
#include <glib.h>

/* Private, GLib-only metadata. GTK's bookmarks remain authoritative. */
GKeyFile *nemo_bookmark_groups_load (const char *filename, GError **error);
gboolean nemo_bookmark_groups_save (GKeyFile *groups, const char *filename, GError **error);
char *nemo_bookmark_groups_filename (void);
char **nemo_bookmark_groups_get_ids (GKeyFile *groups);
char *nemo_bookmark_groups_create (GKeyFile *groups, const char *name);
gboolean nemo_bookmark_groups_rename (GKeyFile *groups, const char *id, const char *name);
void nemo_bookmark_groups_delete (GKeyFile *groups, const char *id);
char *nemo_bookmark_groups_find (GKeyFile *groups, const char *uri);
void nemo_bookmark_groups_move (GKeyFile *groups, const char *uri, const char *id, const char *before_uri);
gboolean nemo_bookmark_groups_prune (GKeyFile *groups, const char * const *uris);
#endif
