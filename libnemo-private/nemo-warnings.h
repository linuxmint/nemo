/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nemo-file-attributes.h: #defines and other file-attribute-related info
 
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

#ifndef NEMO_WARNINGS_H
#define NEMO_WARNINGS_H

#if    __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define NEMO_GNUC_BEGIN_IGNORE_FORMAT_NOLIITERAL		\
  _Pragma ("GCC diagnostic push")			\
  _Pragma ("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define NEMO_GNUC_END_IGNORE_FORMAT_NOLIITERAL			\
  _Pragma ("GCC diagnostic pop")
#elif defined (_MSC_VER) && (_MSC_VER >= 1500)
#define NEMO_GNUC_BEGIN_IGNORE_FORMAT_NOLIITERAL		\
  __pragma (warning (push))  \
  __pragma (warning (disable : 4774 ))
#define NEMO_GNUC_END_IGNORE_FORMAT_NOLIITERAL			\
  __pragma (warning (pop))
#elif defined (__clang__)
#define NEMO_GNUC_BEGIN_IGNORE_FORMAT_NOLIITERAL \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Wformat-nonliteral\"")
#define NEMO_GNUC_END_IGNORE_FORMAT_NOLIITERAL \
  _Pragma("clang diagnostic pop")
#else
#define NEMO_GNUC_BEGIN_IGNORE_FORMAT_NOLIITERAL
#define NEMO_GNUC_END_IGNORE_FORMAT_NOLIITERAL
#endif


#if    __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#define NEMO_GNUC_BEGIN_IGNORE_SUGGESTED_ATTRIBUTE		\
  _Pragma ("GCC diagnostic push")			\
  _Pragma ("GCC diagnostic ignored \"-Wsuggest-attribute\"")
#define NEMO_GNUC_END_IGNORE_SUGGESTED_ATTRIBUTE			\
  _Pragma ("GCC diagnostic pop")
#elif defined (_MSC_VER) && (_MSC_VER >= 1500)
#define NEMO_GNUC_BEGIN_IGNORE_SUGGESTED_ATTRIBUTE		\
  __pragma (warning (push))  \
  // TODO
#define NEMO_GNUC_END_IGNORE_SUGGESTED_ATTRIBUTE			\
  __pragma (warning (pop))
#elif defined (__clang__)
#define NEMO_GNUC_BEGIN_IGNORE_SUGGESTED_ATTRIBUTE \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Wsuggest-attribute\"")
#define NEMO_GNUC_END_IGNORE_SUGGESTED_ATTRIBUTE \
  _Pragma("clang diagnostic pop")
#else
#define NEMO_GNUC_BEGIN_IGNORE_SUGGESTED_ATTRIBUTE
#define NEMO_GNUC_END_IGNORE_SUGGESTED_ATTRIBUTE
#endif


#endif /* NEMO_WARNINGS_H */
