/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-glib-extensions.h - interface for new functions that conceptually
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

#ifndef EEL_GLIB_EXTENSIONS_H
#define EEL_GLIB_EXTENSIONS_H

#include <glib.h>
#include <gio/gio.h>

/* A gboolean variant for bit fields. */
typedef guint eel_boolean_bit;

/* Predicate. */
typedef gboolean (* EelPredicateFunction) (gpointer data,
					   gpointer callback_data);

/* GList functions. */
gboolean    eel_g_lists_sort_and_check_for_intersection (GList                **list_a,
							 GList                **list_b);
GList *     eel_g_list_partition                        (GList                 *list,
							 EelPredicateFunction   predicate,
							 gpointer               user_data,
							 GList                **removed);

/* List functions for lists of C strings. */
gboolean    eel_g_str_list_equal                        (GList                 *str_list_a,
							 GList                 *str_list_b);
GList *     eel_g_str_list_copy                         (GList                 *str_list);

/* List functions for lists of objects */
GList *     eel_g_object_list_copy                      (GList                 *list);

/* GHashTable functions */
void        eel_g_hash_table_safe_for_each              (GHashTable            *hash_table,
							 GHFunc                 callback,
							 gpointer               callback_data);

/* NULL terminated string arrays (strv). */
gboolean    eel_g_strv_equal                            (char                 **a,
							 char                 **b);

#endif /* EEL_GLIB_EXTENSIONS_H */
