/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   eel-lib-self-check-functions.h: Wrapper and prototypes for all
   self-check functions in libeel.
 
   Copyright (C) 2000 Eazel, Inc.
  
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

#include "eel-self-checks.h"

void eel_run_lib_self_checks (void);

/* Putting the prototypes for these self-check functions in each
   header file for the files they are defined in would make compiling
   the self-check framework take way too long (since one file would
   have to include everything).

   So we put the list of functions here instead.

   Instead of just putting prototypes here, we put this macro that
   can be used to do operations on the whole list of functions.
*/

#define EEL_LIB_FOR_EACH_SELF_CHECK_FUNCTION(macro) \
	macro (eel_self_check_glib_extensions) \
	macro (eel_self_check_string) \
/* Add new self-check functions to the list above this line. */

/* Generate prototypes for all the functions. */
EEL_LIB_FOR_EACH_SELF_CHECK_FUNCTION (EEL_SELF_CHECK_FUNCTION_PROTOTYPE)
