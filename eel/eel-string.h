/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-string.h: String routines to augment <string.h>.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef EEL_STRING_H
#define EEL_STRING_H

#include <glib.h>
#include <string.h>
#include <stdarg.h>

/* We use the "str" abbrevation to mean char * string, since
 * "string" usually means g_string instead. We use the "istr"
 * abbreviation to mean a case-insensitive char *.
 */

#define THOU_TO_STR(c) g_strdup_printf ("%'d", c);

/* NULL is allowed for all the str parameters to these functions. */

/* Escape function for '_' character. */
char *   eel_str_double_underscores        (const char    *str);

/* Capitalize a string */
char *   eel_str_capitalize                (const char    *str);

/* Middle truncate a string to a maximum of truncate_length characters.
 * The resulting string will be truncated in the middle with a "..."
 * delimiter.
 */
char *   eel_str_middle_truncate           (const char    *str,
					    guint          truncate_length);


/* Remove all characters after the passed-in substring. */
char *   eel_str_strip_substring_and_after (const char    *str,
					    const char    *substring);

/* Replace all occurrences of substring with replacement. */
char *   eel_str_replace_substring         (const char    *str,
					    const char    *substring,
					    const char    *replacement);

typedef char * eel_ref_str;

eel_ref_str eel_ref_str_new        (const char  *string);
eel_ref_str eel_ref_str_get_unique (const char  *string);
eel_ref_str eel_ref_str_ref        (eel_ref_str  str);
void        eel_ref_str_unref      (eel_ref_str  str);

#define eel_ref_str_peek(__str) ((const char *)(__str))


typedef struct {
  char character;
  char *(*to_string) (char *format, va_list va);
  void (*skip) (va_list *va);
} EelPrintfHandler;

char *eel_strdup_printf_with_custom (EelPrintfHandler *handlers,
				     const char *format,
				     ...);
char *eel_strdup_vprintf_with_custom (EelPrintfHandler *custom,
				      const char *format,
				      va_list va);

#endif /* EEL_STRING_H */
