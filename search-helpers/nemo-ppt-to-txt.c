/* Nemo is free software; you can redistribute it and/or
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
 */

#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>

static void
cleanup_tmp_dir (const gchar *tmp_dir,
                 GFile       *xml_file)
{
    if (tmp_dir == NULL || xml_file == NULL) {
        return;
    }

    GFile *parent;

    parent = g_file_get_parent (xml_file);

    g_file_delete (xml_file, NULL, NULL);
    g_file_delete (parent, NULL, NULL);

    g_object_unref (parent);
}

static gchar *
get_tmp_dir (GError **error)
{
    gchar *tmp_dir = NULL;

    // Create our temp dir in /dev/shm if it's available,
    // otherwise use whatever glib ends up with (/tmp probably).

    if (g_file_test ("/dev/shm", G_FILE_TEST_IS_DIR)) {
        gchar *old_env_tmp;

        old_env_tmp = g_strdup (g_getenv ("TMPDIR"));
        if (g_setenv ("TMPDIR", "/dev/shm", TRUE)) {
            tmp_dir = g_dir_make_tmp ("nemo-search-helper-XXXXXX", NULL);

            if (old_env_tmp != NULL) {
                g_setenv ("TMPDIR", old_env_tmp, TRUE);
                g_free (old_env_tmp);
            } else {
                g_unsetenv ("TMPDIR");
            }
        }
    }

    if (tmp_dir != NULL) {
        return tmp_dir;
    }

    return g_dir_make_tmp ("nemo-search-helper-XXXXXX", error);
}

gchar *
run_regex_replace (const gchar  *pattern,
                   gchar        *input,
                   const gchar  *replacement,
                   GError      **error)
{
    GRegex *re;
    gchar *out;

    out = NULL;

    re = g_regex_new (pattern,
                      G_REGEX_OPTIMIZE,
                      0,
                      error);

    if (re == NULL) {
        return NULL;
    }

    out = g_regex_replace_literal (re, input, -1, 0, replacement, 0, error);
    g_free (input);
    g_regex_unref (re);

    return out;
}

int
main (int argc, char *argv[])
{
    if (argc < 2) {
        g_printerr ("Need a filename\n");
        return 1;
    }

    GSubprocess *lo_proc;
    GFile *xml_file;
    GError *error;

    gchar *tmp_dir = NULL;
    gchar *name_only = NULL;
    gchar *ptr;
    gchar *orig_file_path = NULL, *orig_basename = NULL;
    gchar *xml_file_path = NULL, *xml_basename = NULL;
    gchar *content = NULL;

    gint retval;
    gsize length;

    orig_file_path = g_strdup (argv[1]);

    orig_basename = g_path_get_basename (orig_file_path);
    ptr = g_strrstr (orig_basename, ".");

    name_only = g_strndup (orig_basename, ptr - orig_basename);
    g_free (orig_basename);

    retval = 0;
    xml_file = NULL;
    error = NULL;
    tmp_dir = get_tmp_dir (&error);

    if (tmp_dir == NULL) {
        if (error != NULL) {
            g_warning ("Could not create a temp dir for conversion: %s", error->message);
            g_clear_error (&error);
        }

        retval = 1;
        goto out;
    }

    gchar *lo_args[7] = {
        "libreoffice",
        "--convert-to", "xml",
        "--outdir", tmp_dir,
        orig_file_path,
        NULL
    };

    lo_proc = g_subprocess_newv ((const gchar * const *) lo_args,
                                 G_SUBPROCESS_FLAGS_STDERR_SILENCE | G_SUBPROCESS_FLAGS_STDOUT_SILENCE,
                                 &error);

    if (lo_proc == NULL) {
        if (error != NULL) {
            g_warning ("Could not lauch headless libreoffice for conversion: %s", error->message);
            g_clear_error (&error);
        }
        retval = 1;
        goto out;
    }

    g_subprocess_wait (lo_proc, NULL, &error);
    g_object_unref (lo_proc);
    g_free (orig_file_path);

    if (error != NULL) {
        g_warning ("LibreOffice was unable to convert ppt to xml: %s", error->message);
        g_clear_error (&error);
        retval = 1;
        goto out;
    }

    xml_basename = g_strconcat (name_only, ".xml", NULL);
    xml_file_path = g_build_filename (tmp_dir, xml_basename, NULL);

    xml_file = g_file_new_for_path (xml_file_path);

    if (!g_file_load_contents (xml_file,
                               NULL,
                               &content,
                               &length,
                               NULL,
                               &error)) {
        if (error != NULL) {
            g_warning ("Unable to read xml file: %s", error->message);
            g_clear_error (&error);
            retval = 1;
            goto out;
        }
    }

    // remove doc settings which has content but is uninteresting
    content = run_regex_replace ("<office:settings>[\\s\\S]*?</office:settings>",
                                 content,
                                 "",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    // remove any binary data content like embedded images
    content = run_regex_replace ("<office:binary-data>[\\s\\S]*?</office:binary-data>",
                                 content,
                                 "",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    // remove any escaped markup as content
    content = run_regex_replace ("&lt;[\\s\\S]*?&gt;",
                                 content,
                                 "",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    // remove all remaining markup
    content = run_regex_replace ("<[^>]+>",
                                 content,
                                 " ",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    // remove excess whitespace, replace with a single space
    content = run_regex_replace ("\\s+",
                                 content,
                                 " ",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    g_printf ("%s", content);

out:
    g_free (content);
    g_free (name_only);

    g_free (xml_basename);
    g_free (xml_file_path);

    if (error != NULL)
    {
        g_critical ("Could not extract strings from ppt file: %s", error->message);
        g_error_free (error);
        retval = 1;
    }

    cleanup_tmp_dir (tmp_dir, xml_file);
    g_clear_object (&xml_file);
    g_free (tmp_dir);

    return retval;
}
