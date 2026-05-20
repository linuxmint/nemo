/* nemo-fzy-utils.c - Fuzzy matching utilities for Nemo
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "nemo-fzy-utils.h"
#include "fzy-match.h"

/**
 * nemo_fzy_strip_combining_marks:
 * @str: a UTF-8 string, or %NULL
 * @nfkd_out: (out) (optional) (transfer full): return location for the
 *   intermediate NFKD-normalized form, or %NULL
 *
 * Strips Unicode combining marks after NFKD decomposition, allowing
 * accent-insensitive matching (e.g. "resume" matches "resumé").
 *
 * Returns: (transfer full): a newly allocated string with combining
 *   marks removed, or %NULL if @str is %NULL
 */
char *
nemo_fzy_strip_combining_marks (const char *str, char **nfkd_out)
{
    g_autofree char *nfkd = NULL;
    GString *result;
    const char *p;

    if (nfkd_out != NULL) {
        *nfkd_out = NULL;
    }

    if (str == NULL) {
        return NULL;
    }

    nfkd = g_utf8_normalize (str, -1, G_NORMALIZE_ALL);
    if (nfkd == NULL) {
        return g_strdup (str);
    }

    result = g_string_new (NULL);

    for (p = nfkd; *p != '\0'; p = g_utf8_next_char (p)) {
        gunichar ch = g_utf8_get_char (p);

        if (!g_unichar_ismark (ch)) {
            g_string_append_unichar (result, ch);
        }
    }

    if (nfkd_out != NULL) {
        *nfkd_out = g_steal_pointer (&nfkd);
    }

    return g_string_free (result, FALSE);
}

/**
 * nemo_fzy_match_attrs:
 * @needle: the filter text to match
 * @haystack: the text to match against (e.g. a filename)
 *
 * Performs a fuzzy match of @needle against @haystack with
 * accent-insensitive matching and returns a #PangoAttrList with
 * %PANGO_WEIGHT_BOLD attributes at the matched character positions.
 *
 * Returns: (transfer full) (nullable): a new #PangoAttrList with bold
 *   attributes for matched positions, or %NULL if there is no match.
 *   Free with pango_attr_list_unref().
 */
PangoAttrList *
nemo_fzy_match_attrs (const char *needle, const char *haystack)
{
    g_autofree char *stripped_needle = NULL;
    g_autofree char *stripped_haystack = NULL;
    g_autofree char *nfkd = NULL;
    g_autofree int *orig_offsets = NULL;
    size_t positions[MATCH_MAX_LEN];
    int needle_len, stripped_len, haystack_len;
    PangoAttrList *attrs;
    const char *sp, *hp;

    if (needle == NULL || needle[0] == '\0' || haystack == NULL) {
        return NULL;
    }

    stripped_needle = nemo_fzy_strip_combining_marks (needle, NULL);
    stripped_haystack = nemo_fzy_strip_combining_marks (haystack, &nfkd);

    if (!has_match (stripped_needle, stripped_haystack)) {
        return NULL;
    }

    needle_len = strlen (stripped_needle);
    stripped_len = strlen (stripped_haystack);
    haystack_len = strlen (haystack);

    if (match_positions (stripped_needle, stripped_haystack, positions) == SCORE_MIN) {
        return NULL;
    }

    /* Build stripped byte offset → original byte offset map.
     * The stripped text was created by NFKD-decomposing then removing
     * combining marks. Walk the NFKD form to find base characters
     * (which correspond 1:1 with stripped characters), and track
     * which original character each one came from. */
    orig_offsets = g_new (int, stripped_len + 1);

    {
        const char *np = nfkd ? nfkd : haystack;

        sp = stripped_haystack;
        hp = haystack;

        while (*np != '\0' && *sp != '\0') {
            gunichar nch = g_utf8_get_char (np);

            if (g_unichar_ismark (nch)) {
                np = g_utf8_next_char (np);
                continue;
            }

            int s_off = sp - stripped_haystack;
            int h_off = hp - haystack;
            int s_bytes = g_utf8_next_char (sp) - sp;

            for (int b = 0; b < s_bytes && (s_off + b) < stripped_len; b++) {
                orig_offsets[s_off + b] = h_off;
            }

            sp = g_utf8_next_char (sp);
            np = g_utf8_next_char (np);

            while (*np != '\0' && g_unichar_ismark (g_utf8_get_char (np))) {
                np = g_utf8_next_char (np);
            }

            hp = g_utf8_next_char (hp);
        }

        orig_offsets[stripped_len] = haystack_len;
    }

    attrs = pango_attr_list_new ();

    for (int i = 0; i < needle_len; ) {
        int run_end = i;
        int orig_start, orig_end;
        PangoAttribute *attr;

        while (run_end + 1 < needle_len &&
               positions[run_end + 1] == positions[run_end] + 1) {
            run_end++;
        }

        orig_start = orig_offsets[positions[i]];
        orig_end = (positions[run_end] + 1 <= (size_t) stripped_len)
                   ? orig_offsets[positions[run_end] + 1]
                   : haystack_len;

        attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
        attr->start_index = orig_start;
        attr->end_index = orig_end;
        pango_attr_list_insert (attrs, attr);

        i = run_end + 1;
    }

    return attrs;
}
