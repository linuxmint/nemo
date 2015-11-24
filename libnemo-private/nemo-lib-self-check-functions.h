/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-lib-self-check-functions.h: Wrapper and prototypes for all
   self-check functions in libnemo.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <eel/eel-self-checks.h>

void nemo_run_lib_self_checks (void);

/* Putting the prototypes for these self-check functions in each
   header file for the files they are defined in would make compiling
   the self-check framework take way too long (since one file would
   have to include everything).

   So we put the list of functions here instead.

   Instead of just putting prototypes here, we put this macro that
   can be used to do operations on the whole list of functions.
*/

#define NEMO_LIB_FOR_EACH_SELF_CHECK_FUNCTION(macro) \
	macro (nemo_self_check_file_utilities) \
	macro (nemo_self_check_file_operations) \
	macro (nemo_self_check_directory) \
	macro (nemo_self_check_file) \
	macro (nemo_self_check_canvas_container) \
/* Add new self-check functions to the list above this line. */

/* Generate prototypes for all the functions. */
NEMO_LIB_FOR_EACH_SELF_CHECK_FUNCTION (EEL_SELF_CHECK_FUNCTION_PROTOTYPE)
