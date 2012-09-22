/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-glib-extensions.c - implementation of new functions that conceptually
                                belong in glib. Perhaps some of these will be
                                actually rolled into glib someday.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "eel-glib-extensions.h"

#include "eel-debug.h"
#include "eel-lib-self-check-functions.h"
#include "eel-string.h"
#include <glib-object.h>
#include <math.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <locale.h>

/* Legal conversion specifiers, as specified in the C standard. */
#define C_STANDARD_STRFTIME_CHARACTERS "aAbBcdHIjmMpSUwWxXyYZ"
#define C_STANDARD_NUMERIC_STRFTIME_CHARACTERS "dHIjmMSUwWyY"
#define SUS_EXTENDED_STRFTIME_MODIFIERS "EO"

#define SAFE_SHELL_CHARACTERS "-_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

/**
 * eel_g_date_new_tm:
 * 
 * Get a new GDate * for the date represented by a tm struct. 
 * The caller is responsible for g_free-ing the result.
 * @time_pieces: Pointer to a tm struct representing the date to be converted.
 * 
 * Returns: Newly allocated date.
 * 
 **/
GDate *
eel_g_date_new_tm (struct tm *time_pieces)
{
	/* tm uses 0-based months; GDate uses 1-based months.
	 * tm_year needs 1900 added to get the full year.
	 */
	return g_date_new_dmy (time_pieces->tm_mday,
			       time_pieces->tm_mon + 1,
			       time_pieces->tm_year + 1900);
}

/**
 * eel_strdup_strftime:
 *
 * Cover for standard date-and-time-formatting routine strftime that returns
 * a newly-allocated string of the correct size. The caller is responsible
 * for g_free-ing the returned string.
 *
 * Besides the buffer management, there are two differences between this
 * and the library strftime:
 *
 *   1) The modifiers "-" and "_" between a "%" and a numeric directive
 *      are defined as for the GNU version of strftime. "-" means "do not
 *      pad the field" and "_" means "pad with spaces instead of zeroes".
 *   2) Non-ANSI extensions to strftime are flagged at runtime with a
 *      warning, so it's easy to notice use of the extensions without
 *      testing with multiple versions of the library.
 *
 * @format: format string to pass to strftime. See strftime documentation
 * for details.
 * @time_pieces: date/time, in struct format.
 * 
 * Return value: Newly allocated string containing the formatted time.
 **/
char *
eel_strdup_strftime (const char *format, struct tm *time_pieces)
{
	GString *string;
	const char *remainder, *percent;
	char code[4], buffer[512];
	char *piece, *result, *converted;
	size_t string_length;
	gboolean strip_leading_zeros, turn_leading_zeros_to_spaces;
	char modifier;
	int i;

	/* Format could be translated, and contain UTF-8 chars,
	 * so convert to locale encoding which strftime uses */
	converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
	g_return_val_if_fail (converted != NULL, NULL);
	
	string = g_string_new ("");
	remainder = converted;

	/* Walk from % character to % character. */
	for (;;) {
		percent = strchr (remainder, '%');
		if (percent == NULL) {
			g_string_append (string, remainder);
			break;
		}
		g_string_append_len (string, remainder,
				     percent - remainder);

		/* Handle the "%" character. */
		remainder = percent + 1;
		switch (*remainder) {
		case '-':
			strip_leading_zeros = TRUE;
			turn_leading_zeros_to_spaces = FALSE;
			remainder++;
			break;
		case '_':
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = TRUE;
			remainder++;
			break;
		case '%':
			g_string_append_c (string, '%');
			remainder++;
			continue;
		case '\0':
			g_warning ("Trailing %% passed to eel_strdup_strftime");
			g_string_append_c (string, '%');
			continue;
		default:
			strip_leading_zeros = FALSE;
			turn_leading_zeros_to_spaces = FALSE;
			break;
		}

		modifier = 0;
		if (strchr (SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL) {
			modifier = *remainder;
			remainder++;

			if (*remainder == 0) {
				g_warning ("Unfinished %%%c modifier passed to eel_strdup_strftime", modifier);
				break;
			}
		} 
		
		if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL) {
			g_warning ("eel_strdup_strftime does not support "
				   "non-standard escape code %%%c",
				   *remainder);
		}

		/* Convert code to strftime format. We have a fixed
		 * limit here that each code can expand to a maximum
		 * of 512 bytes, which is probably OK. There's no
		 * limit on the total size of the result string.
		 */
		i = 0;
		code[i++] = '%';
		if (modifier != 0) {
#ifdef HAVE_STRFTIME_EXTENSION
			code[i++] = modifier;
#endif
		}
		code[i++] = *remainder;
		code[i++] = '\0';
		string_length = strftime (buffer, sizeof (buffer),
					  code, time_pieces);
		if (string_length == 0) {
			/* We could put a warning here, but there's no
			 * way to tell a successful conversion to
			 * empty string from a failure.
			 */
			buffer[0] = '\0';
		}

		/* Strip leading zeros if requested. */
		piece = buffer;
		if (strip_leading_zeros || turn_leading_zeros_to_spaces) {
			if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL) {
				g_warning ("eel_strdup_strftime does not support "
					   "modifier for non-numeric escape code %%%c%c",
					   remainder[-1],
					   *remainder);
			}
			if (*piece == '0') {
				do {
					piece++;
				} while (*piece == '0');
				if (!g_ascii_isdigit (*piece)) {
				    piece--;
				}
			}
			if (turn_leading_zeros_to_spaces) {
				memset (buffer, ' ', piece - buffer);
				piece = buffer;
			}
		}
		remainder++;

		/* Add this piece. */
		g_string_append (string, piece);
	}
	
	/* Convert the string back into utf-8. */
	result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

	g_string_free (string, TRUE);
	g_free (converted);

	return result;
}

/**
 * eel_g_list_exactly_one_item
 *
 * Like g_list_length (list) == 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has exactly one item.
 **/
gboolean
eel_g_list_exactly_one_item (GList *list)
{
	return list != NULL && list->next == NULL;
}

/**
 * eel_g_list_more_than_one_item
 *
 * Like g_list_length (list) > 1, only O(1) instead of O(n).
 * @list: List.
 *
 * Return value: TRUE if the list has more than one item.
 **/
gboolean
eel_g_list_more_than_one_item (GList *list)
{
	return list != NULL && list->next != NULL;
}

/**
 * eel_g_list_equal
 *
 * Compares two lists to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists are the same length with the same elements.
 **/
gboolean
eel_g_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (p->data != q->data) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_equal
 *
 * Compares two lists of C strings to see if they are equal.
 * @list_a: First list.
 * @list_b: Second list.
 *
 * Return value: TRUE if the lists contain the same strings.
 **/
gboolean
eel_g_str_list_equal (GList *list_a, GList *list_b)
{
	GList *p, *q;

	for (p = list_a, q = list_b; p != NULL && q != NULL; p = p->next, q = q->next) {
		if (g_strcmp0 (p->data, q->data) != 0) {
			return FALSE;
		}
	}
	return p == NULL && q == NULL;
}

/**
 * eel_g_str_list_copy
 *
 * @list: List of strings and/or NULLs to copy.
 * Return value: Deep copy of @list.
 **/
GList *
eel_g_str_list_copy (GList *list)
{
	GList *node, *result;

	result = NULL;
	
	for (node = g_list_last (list); node != NULL; node = node->prev) {
		result = g_list_prepend (result, g_strdup (node->data));
	}
	return result;
}

/**
 * eel_g_str_list_alphabetize
 *
 * Sort a list of strings using locale-sensitive rules.
 *
 * @list: List of strings and/or NULLs.
 * 
 * Return value: @list, sorted.
 **/
GList *
eel_g_str_list_alphabetize (GList *list)
{
	return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

int
eel_g_str_list_index (GList *str_list,
		      const char *str)
{
	int i;
	GList *l;
	for (i = 0, l = str_list; l != NULL; l = l->next, i++) {
		if (!strcmp (str, (const char*)l->data)) {
			return i;
		}
	}
	return -1;
}

/**
 * eel_g_strv_find
 * 
 * Get index of string in array of strings.
 * 
 * @strv: NULL-terminated array of strings.
 * @find_me: string to search for.
 * 
 * Return value: index of array entry in @strv that
 * matches @find_me, or -1 if no matching entry.
 */
int
eel_g_strv_find (char **strv, const char *find_me)
{
	int index;

	g_return_val_if_fail (find_me != NULL, -1);
	
	for (index = 0; strv[index] != NULL; ++index) {
		if (strcmp (strv[index], find_me) == 0) {
			return index;
		}
	}

	return -1;
}

gboolean
eel_g_strv_equal (char **a, char **b)
{
	int i;

	if (g_strv_length (a) != g_strv_length (b)) {
		return FALSE;
	}

	for (i = 0; a[i] != NULL; i++) {
		if (strcmp (a[i], b[i]) != 0) {
			return FALSE;
		}
	}
	return TRUE;
}

static int
compare_pointers (gconstpointer pointer_1, gconstpointer pointer_2)
{
	if ((const char *) pointer_1 < (const char *) pointer_2) {
		return -1;
	}
	if ((const char *) pointer_1 > (const char *) pointer_2) {
		return +1;
	}
	return 0;
}

gboolean
eel_g_lists_sort_and_check_for_intersection (GList **list_1,
					     GList **list_2) 

{
	GList *node_1, *node_2;
	int compare_result;
	
	*list_1 = g_list_sort (*list_1, compare_pointers);
	*list_2 = g_list_sort (*list_2, compare_pointers);

	node_1 = *list_1;
	node_2 = *list_2;

	while (node_1 != NULL && node_2 != NULL) {
		compare_result = compare_pointers (node_1->data, node_2->data);
		if (compare_result == 0) {
			return TRUE;
		}
		if (compare_result <= 0) {
			node_1 = node_1->next;
		}
		if (compare_result >= 0) {
			node_2 = node_2->next;
		}
	}

	return FALSE;
}


/**
 * eel_g_list_partition
 * 
 * Parition a list into two parts depending on whether the data
 * elements satisfy a provided predicate. Order is preserved in both
 * of the resulting lists, and the original list is consumed. A list
 * of the items that satisfy the predicate is returned, and the list
 * of items not satisfying the predicate is returned via the failed
 * out argument.
 * 
 * @list: List to partition.
 * @predicate: Function to call on each element.
 * @user_data: Data to pass to function.  
 * @failed: The GList * variable pointed to by this argument will be
 * set to the list of elements for which the predicate returned
 * false. */

GList *
eel_g_list_partition (GList *list,
			   EelPredicateFunction  predicate,
			   gpointer user_data,
			   GList **failed)
{
	GList *predicate_true;
	GList *predicate_false;
	GList *reverse;
	GList *p;
	GList *next;

	predicate_true = NULL;
	predicate_false = NULL;

	reverse = g_list_reverse (list);

	for (p = reverse; p != NULL; p = next) {
		next = p->next;
		
		if (next != NULL) {
			next->prev = NULL;
		}

		if (predicate (p->data, user_data)) {
			p->next = predicate_true;
 			if (predicate_true != NULL) {
				predicate_true->prev = p;
			}
			predicate_true = p;
		} else {
			p->next = predicate_false;
 			if (predicate_false != NULL) {
				predicate_false->prev = p;
			}
			predicate_false = p;
		}
	}

	*failed = predicate_false;
	return predicate_true;
}

/**
 * eel_get_system_time
 * 
 * Return value: number of microseconds since the machine was turned on
 */
gint64
eel_get_system_time (void)
{
	struct timeval tmp;

	gettimeofday (&tmp, NULL);
	return (gint64)tmp.tv_usec + (gint64)tmp.tv_sec * G_GINT64_CONSTANT (1000000);
}

typedef struct {
	GList *keys;
	GList *values;
} FlattenedHashTable;

static void
flatten_hash_table_element (gpointer key, gpointer value, gpointer callback_data)
{
	FlattenedHashTable *flattened_table;

	flattened_table = callback_data;
	flattened_table->keys = g_list_prepend
		(flattened_table->keys, key);
	flattened_table->values = g_list_prepend
		(flattened_table->values, value);
}

void
eel_g_hash_table_safe_for_each (GHashTable *hash_table,
				GHFunc callback,
				gpointer callback_data)
{
	FlattenedHashTable flattened;
	GList *p, *q;

	flattened.keys = NULL;
	flattened.values = NULL;

	g_hash_table_foreach (hash_table,
			      flatten_hash_table_element,
			      &flattened);

	for (p = flattened.keys, q = flattened.values;
	     p != NULL;
	     p = p->next, q = q->next) {
		(* callback) (p->data, q->data, callback_data);
	}

	g_list_free (flattened.keys);
	g_list_free (flattened.values);
}

int
eel_round (double d)
{
	double val;

	val = floor (d + .5);

	/* The tests are needed because the result of floating-point to integral
	 * conversion is undefined if the floating point value is not representable
	 * in the new type. E.g. the magnititude is too large or a negative
	 * floating-point value being converted to an unsigned.
	 */
	g_return_val_if_fail (val <= INT_MAX, INT_MAX);
	g_return_val_if_fail (val >= INT_MIN, INT_MIN);

	return val;
}

/**
 * eel_g_object_list_ref
 *
 * Ref all the objects in a list.
 * @list: GList of objects.
 **/
GList *
eel_g_object_list_ref (GList *list)
{
	g_list_foreach (list, (GFunc) g_object_ref, NULL);
	return list;
}

/**
 * eel_g_object_list_copy
 *
 * Copy the list of objects, ref'ing each one.
 * @list: GList of objects.
 **/
GList *
eel_g_object_list_copy (GList *list)
{
	return g_list_copy (eel_g_object_list_ref (list));
}

/**
 * eel_add_weak_pointer
 *
 * Nulls out a saved reference to an object when the object gets destroyed.
 *
 * @pointer_location: Address of the saved pointer.
 **/
void 
eel_add_weak_pointer (gpointer pointer_location)
{
	gpointer *object_location;

	g_return_if_fail (pointer_location != NULL);

	object_location = (gpointer *) pointer_location;
	if (*object_location == NULL) {
		/* The reference is NULL, nothing to do. */
		return;
	}

	g_return_if_fail (G_IS_OBJECT (*object_location));

	g_object_add_weak_pointer (G_OBJECT (*object_location),
				   object_location);
}

/**
 * eel_remove_weak_pointer
 *
 * Removes the weak pointer that was added by eel_add_weak_pointer.
 * Also nulls out the pointer.
 *
 * @pointer_location: Pointer that was passed to eel_add_weak_pointer.
 **/
void 
eel_remove_weak_pointer (gpointer pointer_location)
{
	gpointer *object_location;

	g_return_if_fail (pointer_location != NULL);

	object_location = (gpointer *) pointer_location;	
	if (*object_location == NULL) {
		/* The object was already destroyed and the reference
		 * nulled out, nothing to do.
		 */
		return;
	}

	g_return_if_fail (G_IS_OBJECT (*object_location));

	g_object_remove_weak_pointer (G_OBJECT (*object_location),
				      object_location);
	
	*object_location = NULL;
}

static void
update_auto_strv_as_quarks (GSettings   *settings,
			    const gchar *key,
			    gpointer     user_data)
{
	GQuark **storage = user_data;
	int i = 0;
	char **value;

	value = g_settings_get_strv (settings, key);

	g_free (*storage);
	*storage = g_new (GQuark, g_strv_length (value) + 1);

	for (i = 0; value[i] != NULL; ++i) {
		(*storage)[i] = g_quark_from_string (value[i]);
	}
	(*storage)[i] = 0;

	g_strfreev (value);
}

void
eel_g_settings_add_auto_strv_as_quarks (GSettings *settings,
					const char *key,
					GQuark **storage)
{
	char *signal;

	*storage = NULL;
	update_auto_strv_as_quarks (settings, key, storage);
	signal = g_strconcat ("changed::", key, NULL);
	g_signal_connect (settings, signal,
			  G_CALLBACK(update_auto_strv_as_quarks),
			  storage);

	g_free (signal);
}

#if !defined (EEL_OMIT_SELF_CHECK)

static void
check_tm_to_g_date (time_t time)
{
	struct tm *before_conversion;
	struct tm after_conversion;
	GDate *date;

	before_conversion = localtime (&time);
	date = eel_g_date_new_tm (before_conversion);

	g_date_to_struct_tm (date, &after_conversion);

	g_date_free (date);

	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_mday,
				       before_conversion->tm_mday);
	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_mon,
				       before_conversion->tm_mon);
	EEL_CHECK_INTEGER_RESULT (after_conversion.tm_year,
				       before_conversion->tm_year);
}

static gboolean
eel_test_predicate (gpointer data,
		    gpointer callback_data)
{
	return g_ascii_strcasecmp (data, callback_data) <= 0;
}

static char *
test_strftime (const char *format,
	       int year,
	       int month,
	       int day,
	       int hour,
	       int minute,
	       int second)
{
	struct tm time_pieces;

	time_pieces.tm_sec = second;
	time_pieces.tm_min = minute;
	time_pieces.tm_hour = hour;
	time_pieces.tm_mday = day;
	time_pieces.tm_mon = month - 1;
	time_pieces.tm_year = year - 1900;
	time_pieces.tm_isdst = -1;
	mktime (&time_pieces);

	return eel_strdup_strftime (format, &time_pieces);
}

void
eel_self_check_glib_extensions (void)
{
	char **strv;
	GList *compare_list_1;
	GList *compare_list_2;
	GList *compare_list_3;
	GList *compare_list_4;
	GList *compare_list_5;
	gint64 time1, time2;
	GList *list_to_partition;
	GList *expected_passed;
	GList *expected_failed;
	GList *actual_passed;
	GList *actual_failed;
	char *huge_string;
	
	check_tm_to_g_date (0);			/* lower limit */
	check_tm_to_g_date ((time_t) -1);	/* upper limit */
	check_tm_to_g_date (time (NULL));	/* current time */

	strv = g_strsplit ("zero|one|two|three|four", "|", 0);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "zero"), 0);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "one"), 1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "four"), 4);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "five"), -1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, ""), -1);
	EEL_CHECK_INTEGER_RESULT (eel_g_strv_find (strv, "o"), -1);
	g_strfreev (strv);

	/* eel_get_system_time */
	time1 = eel_get_system_time ();
	time2 = eel_get_system_time ();
	EEL_CHECK_BOOLEAN_RESULT (time1 - time2 > -1000, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (time1 - time2 <= 0, TRUE);

	/* eel_g_str_list_equal */

	/* We g_strdup because identical string constants can be shared. */

	compare_list_1 = NULL;
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("Apple"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("zebra"));
	compare_list_1 = g_list_append (compare_list_1, g_strdup ("!@#!@$#@$!"));

	compare_list_2 = NULL;
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("Apple"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("zebra"));
	compare_list_2 = g_list_append (compare_list_2, g_strdup ("!@#!@$#@$!"));

	compare_list_3 = NULL;
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("Apple"));
	compare_list_3 = g_list_append (compare_list_3, g_strdup ("zebra"));

	compare_list_4 = NULL;
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("Apple"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("zebra"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("!@#!@$#@$!"));
	compare_list_4 = g_list_append (compare_list_4, g_strdup ("foobar"));

	compare_list_5 = NULL;
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("Apple"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("zzzzzebraaaaaa"));
	compare_list_5 = g_list_append (compare_list_5, g_strdup ("!@#!@$#@$!"));

	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_2), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_3), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_4), FALSE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (compare_list_1, compare_list_5), FALSE);

	g_list_free_full (compare_list_1, g_free);
	g_list_free_full (compare_list_2, g_free);
	g_list_free_full (compare_list_3, g_free);
	g_list_free_full (compare_list_4, g_free);
	g_list_free_full (compare_list_5, g_free);

	/* eel_g_list_partition */

	list_to_partition = NULL;
	list_to_partition = g_list_append (list_to_partition, "Cadillac");
	list_to_partition = g_list_append (list_to_partition, "Pontiac");
	list_to_partition = g_list_append (list_to_partition, "Ford");
	list_to_partition = g_list_append (list_to_partition, "Range Rover");
	
	expected_passed = NULL;
	expected_passed = g_list_append (expected_passed, "Cadillac");
	expected_passed = g_list_append (expected_passed, "Ford");
	
	expected_failed = NULL;
	expected_failed = g_list_append (expected_failed, "Pontiac");
	expected_failed = g_list_append (expected_failed, "Range Rover");
	
	actual_passed = eel_g_list_partition (list_to_partition, 
						   eel_test_predicate,
						   "m",
						   &actual_failed);
	
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_passed, actual_passed), TRUE);
	EEL_CHECK_BOOLEAN_RESULT (eel_g_str_list_equal (expected_failed, actual_failed), TRUE);
	
	/* Don't free "list_to_partition", since it is consumed
	 * by eel_g_list_partition.
	 */
	
	g_list_free (expected_passed);
	g_list_free (actual_passed);
	g_list_free (expected_failed);
	g_list_free (actual_failed);

	/* eel_strdup_strftime */
	huge_string = g_new (char, 10000+1);
	memset (huge_string, 'a', 10000);
	huge_string[10000] = '\0';

	setlocale (LC_TIME, "C");

	EEL_CHECK_STRING_RESULT (test_strftime ("", 2000, 1, 1, 0, 0, 0), "");
	EEL_CHECK_STRING_RESULT (test_strftime (huge_string, 2000, 1, 1, 0, 0, 0), huge_string);
	EEL_CHECK_STRING_RESULT (test_strftime ("%%", 2000, 1, 1, 1, 0, 0), "%");
	EEL_CHECK_STRING_RESULT (test_strftime ("%%%%", 2000, 1, 1, 1, 0, 0), "%%");
	EEL_CHECK_STRING_RESULT (test_strftime ("%m/%d/%y, %I:%M %p", 2000, 1, 1, 1, 0, 0), "01/01/00, 01:00 AM");
	EEL_CHECK_STRING_RESULT (test_strftime ("%-m/%-d/%y, %-I:%M %p", 2000, 1, 1, 1, 0, 0), "1/1/00, 1:00 AM");
	EEL_CHECK_STRING_RESULT (test_strftime ("%_m/%_d/%y, %_I:%M %p", 2000, 1, 1, 1, 0, 0), " 1/ 1/00,  1:00 AM");

	setlocale (LC_TIME, "");

	g_free (huge_string);
}

#endif /* !EEL_OMIT_SELF_CHECK */
