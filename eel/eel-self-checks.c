/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
   
   eel-self-checks.c: The self-check framework.
 
   Copyright (C) 1999 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
  
   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>

#if ! defined (EEL_OMIT_SELF_CHECK)

#include "eel-self-checks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gboolean failed;

static const char *current_expression;
static const char *current_file_name;
static int current_line_number;

void
eel_exit_if_self_checks_failed (void)
{
	if (!failed) {
		return;
	}

	printf ("\n");

	exit (EXIT_FAILURE);
}

void
eel_report_check_failure (char *result, char *expected)
{
	if (!failed) {
		fprintf (stderr, "\n");
	}

	fprintf (stderr, "FAIL: check failed in %s, line %d\n", current_file_name, current_line_number);
	fprintf (stderr, "      evaluated: %s\n", current_expression);
	fprintf (stderr, "       expected: %s\n", expected == NULL ? "NULL" : expected);
	fprintf (stderr, "            got: %s\n", result == NULL ? "NULL" : result);
	
	failed = TRUE;

	g_free (result);
	g_free (expected);
}

static char *
eel_strdup_boolean (gboolean boolean)
{
	if (boolean == FALSE) {
		return g_strdup ("FALSE");
	}
	if (boolean == TRUE) {
		return g_strdup ("TRUE");
	}
	return g_strdup_printf ("gboolean(%d)", boolean);
}

void
eel_before_check (const char *expression,
		  const char *file_name,
		  int line_number)
{
	current_expression = expression;
	current_file_name = file_name;
	current_line_number = line_number;
}

void
eel_after_check (void)
{
	/* It would be good to check here if there was a memory leak. */
}

void
eel_check_boolean_result (gboolean result, gboolean expected)
{
	if (result != expected) {
		eel_report_check_failure (eel_strdup_boolean (result),
					  eel_strdup_boolean (expected));
	}
	eel_after_check ();
}

void
eel_check_rectangle_result (EelIRect result,
			    int expected_x0,
			    int expected_y0,
			    int expected_x1,
			    int expected_y1)
{
	if (result.x0 != expected_x0
	    || result.y0 != expected_y0
	    || result.x1 != expected_x1
	    || result.y1 != expected_y1) {
		eel_report_check_failure (g_strdup_printf ("x0=%d, y0=%d, x1=%d, y1=%d",
							   result.x0,
							   result.y0,
							   result.x1,
							   result.y1),
					  g_strdup_printf ("x0=%d, y0=%d, x1=%d, y1=%d",
							   expected_x0,
							   expected_y0,
							   expected_x1,
							   expected_y1));
	}
	eel_after_check ();
}

void
eel_check_integer_result (long result, long expected)
{
	if (result != expected) {
		eel_report_check_failure (g_strdup_printf ("%ld", result),
					  g_strdup_printf ("%ld", expected));
	}
	eel_after_check ();
}

void
eel_check_double_result (double result, double expected)
{
	if (result != expected) {
		eel_report_check_failure (g_strdup_printf ("%f", result),
					  g_strdup_printf ("%f", expected));
	}
	eel_after_check ();
}

void
eel_check_string_result (char *result, const char *expected)
{
	gboolean match;
	
	/* Stricter than eel_strcmp.
	 * NULL does not match "" in this test.
	 */
	if (expected == NULL) {
		match = result == NULL;
	} else {
		match = result != NULL && strcmp (result, expected) == 0;
	}

	if (!match) {
		eel_report_check_failure (result, g_strdup (expected));
	} else {
		g_free (result);
	}
	eel_after_check ();
}

void
eel_before_check_function (const char *name)
{
	fprintf (stderr, "running %s\n", name);
}

void
eel_after_check_function (void)
{
}

#endif /* ! EEL_OMIT_SELF_CHECK */
