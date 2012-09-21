/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-debug.c: Eel debugging aids.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
#include "eel-debug.h"

#include <glib.h>
#include <signal.h>
#include <stdio.h>

typedef struct {
	gpointer data;
	GFreeFunc function;
} ShutdownFunction;

static GList *shutdown_functions;

/* Raise a SIGINT signal to get the attention of the debugger.
 * When not running under the debugger, we don't want to stop,
 * so we ignore the signal for just the moment that we raise it.
 */
static void
eel_stop_in_debugger (void)
{
	void (* saved_handler) (int);

	saved_handler = signal (SIGINT, SIG_IGN);
	raise (SIGINT);
	signal (SIGINT, saved_handler);
}

/* Stop in the debugger after running the default log handler.
 * This makes certain kinds of messages stop in the debugger
 * without making them fatal (you can continue).
 */
static void
log_handler (const char *domain,
	     GLogLevelFlags level,
	     const char *message,
	     gpointer data)
{
	g_log_default_handler (domain, level, message, data);
	if ((level & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING)) != 0) {
		eel_stop_in_debugger ();
	}
}

void
eel_make_warnings_and_criticals_stop_in_debugger (void)
{
	g_log_set_default_handler (log_handler, NULL);
}

void
eel_debug_shut_down (void)
{
	ShutdownFunction *f;

	while (shutdown_functions != NULL) {
		f = shutdown_functions->data;
		shutdown_functions = g_list_remove (shutdown_functions, f);
		
		f->function (f->data);
		g_free (f);
	}
}

void
eel_debug_call_at_shutdown (EelFunction function)
{
	eel_debug_call_at_shutdown_with_data ((GFreeFunc) function, NULL);
}

void
eel_debug_call_at_shutdown_with_data (GFreeFunc function, gpointer data)
{
	ShutdownFunction *f;

	f = g_new (ShutdownFunction, 1);
	f->data = data;
	f->function = function;
	shutdown_functions = g_list_prepend (shutdown_functions, f);
}
