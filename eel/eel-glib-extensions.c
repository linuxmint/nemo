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

/**
 * eel_g_object_list_copy
 *
 * Copy the list of objects, ref'ing each one.
 * @list: GList of objects.
 **/
GList *
eel_g_object_list_copy (GList *list)
{
	g_list_foreach (list, (GFunc) g_object_ref, NULL);
	return g_list_copy (list);
}

#if !defined (EEL_OMIT_SELF_CHECK)

static gboolean
eel_test_predicate (gpointer data,
		    gpointer callback_data)
{
	return g_ascii_strcasecmp (data, callback_data) <= 0;
}

void
eel_self_check_glib_extensions (void)
{
	GList *compare_list_1;
	GList *compare_list_2;
	GList *compare_list_3;
	GList *compare_list_4;
	GList *compare_list_5;
	GList *list_to_partition;
	GList *expected_passed;
	GList *expected_failed;
	GList *actual_passed;
	GList *actual_failed;
	
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
}

#endif /* !EEL_OMIT_SELF_CHECK */
