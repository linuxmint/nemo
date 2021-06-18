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
#include <glib/gprintf.h>
#include <libgsf-1/gsf/gsf.h>

static const gchar *SKIPFILES[] = {
    "styles.xml",
    "theme",
    "_rels",
    "printerSettings",
    "media",
    "drawings"
    // docx
    "settings.xml",
    "app.xml",
    "theme1.xml",
    "[Content_Types].xml",
    "fontTable.xml",
    "webSettings.xml",
    // xlsx
    "worksheets",
    "calcChain.xml",
    // pptx
    "slideLayouts",
    "slideMasters",
    "presProps.xml",
    "tableStyles.xml",
    "viewProps.xml",
    "presentation.xml",
    NULL
};

static void
process_file (GString   *collective,
              GsfInfile *file)
{
    GString *contents;
    gsf_off_t remaining;

    contents = g_string_new (NULL);
    remaining = gsf_input_size (GSF_INPUT (file));

    do {
        gint size = MIN (remaining, 1024);
        guint8 chunk[size];

        gsf_input_read (GSF_INPUT (file), size, chunk);

        if (chunk != NULL)
        {
            remaining -= size;
            contents = g_string_append_len (contents, (const gchar *) chunk, size);
        }
    } while (remaining > 0);

    g_string_append (contents, " ");
    g_string_append (collective, contents->str);
    g_string_free (contents, TRUE);
}

static void
iterate_thru_levels (GString   *collective,
                     GsfInfile *infile)
{
    gint n_children, i;

    n_children = gsf_infile_num_children (infile);

    for (i = 0; i < n_children; i++)
    {
        GsfInfile *child;
        const gchar *name;
        gint n_child_children;

        child = GSF_INFILE (gsf_infile_child_by_index (infile, i));
        name = gsf_infile_name_by_index (infile, i);

        if (g_strv_contains (SKIPFILES, name)) {
            g_object_unref (child);
            continue;
        }

        n_child_children = gsf_infile_num_children (child);

        if (n_child_children < 0 && g_str_has_suffix (name, ".xml"))
        {
            process_file (collective, child);
        }
        else
        if (n_child_children > 0)
        {
            iterate_thru_levels (collective, child);
        }

        g_object_unref (child);
    }
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
    GsfInput *input;
    GsfInfile *toplevel;
    GString *collective;
    GError *error;
    GFile *file;
    gchar *filename;
    gchar *content;

    if (argc < 2) {
        g_printerr ("Need a filename\n");
        return 1;
    }

    filename = g_strdup (argv[1]);
    file = g_file_new_for_path (filename);
    g_free (filename);

    error = NULL;
    input = gsf_input_gio_new (file, &error);
    g_object_unref (file);

    if (error != NULL)
    {
        g_critical ("Could not open mso file for reading: %s", error->message);
        g_error_free (error);
        return 1;
    }

    toplevel = gsf_infile_zip_new (input, &error);

    if (error != NULL)
    {
        g_critical ("Could not load mso file: %s", error->message);
        g_object_unref (input);
        g_error_free (error);
        return 1;
    }

    collective = g_string_new (NULL);

    iterate_thru_levels (collective, GSF_INFILE (toplevel));

    g_object_unref (toplevel);
    g_object_unref (input);

    content = g_string_free (collective, FALSE);

    if (content == NULL) {
        goto out;
    }

    content = run_regex_replace ("<[^>]+>",
                                 content,
                                 "",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    content = run_regex_replace ("\\s+",
                                 content,
                                 " ",
                                 &error);

    if (content == NULL) {
        goto out;
    }

    g_printf ("%s", content);
    g_free (content);

out:
    if (error != NULL)
    {
        g_critical ("Could not extract strings from mso 2003+ file: %s", error->message);
        g_error_free (error);
        return 1;
    }

    return 0;
}
