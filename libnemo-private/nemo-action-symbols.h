/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#ifndef NEMO_ACTION_SYMBOLS_H
#define NEMO_ACTION_SYMBOLS_H

/**
 * List of uris
 * */
#define TOKEN_EXEC_URI_LIST "%U"

/**
 * List of paths
 * */
#define TOKEN_EXEC_FILE_LIST "%F"
#define TOKEN_EXEC_LOCATION_PATH "%P" // also parent path
#define TOKEN_EXEC_LOCATION_URI "%R" //  and uri
#define TOKEN_EXEC_FILE_NAME "%f"
#define TOKEN_EXEC_PARENT_NAME "%p"
#define TOKEN_EXEC_DEVICE "%D"
#define TOKEN_EXEC_FILE_NO_EXT "%e"
#define TOKEN_EXEC_LITERAL_PERCENT "%%"
#define TOKEN_EXEC_XID "%X"

#define TOKEN_LABEL_FILE_NAME "%N" // Leave in for compatibility, same as TOKEN_EXEC_FILE_NAME


#define SELECTION_SINGLE_KEY "s"
#define SELECTION_MULTIPLE_KEY "m"
#define SELECTION_ANY_KEY "any"
#define SELECTION_NONE_KEY "none"
#define SELECTION_NOT_NONE_KEY "notnone"

typedef enum {
    SELECTION_SINGLE = G_MAXINT - 10,
    SELECTION_MULTIPLE,
    SELECTION_NOT_NONE,
    SELECTION_ANY,
    SELECTION_NONE
} SelectionType;

typedef enum {
    QUOTE_TYPE_SINGLE = 0,
    QUOTE_TYPE_DOUBLE,
    QUOTE_TYPE_BACKTICK,
    QUOTE_TYPE_NONE
} QuoteType;

typedef enum {
    TOKEN_NONE = 0,
    TOKEN_PATH_LIST,
    TOKEN_URI_LIST,
    TOKEN_FILE_DISPLAY_NAME,
    TOKEN_PARENT_DISPLAY_NAME,
    TOKEN_PARENT_PATH,
    TOKEN_PARENT_URI,
    TOKEN_DEVICE,
    TOKEN_FILE_DISPLAY_NAME_NO_EXT,
    TOKEN_LITERAL_PERCENT,
    TOKEN_XID
} TokenType;

#define ACTION_FILE_GROUP "Nemo Action"

#define KEY_ACTIVE "Active"
#define KEY_NAME "Name"
#define KEY_COMMENT "Comment"
#define KEY_EXEC "Exec"
#define KEY_ICON_NAME "Icon-Name"
#define KEY_STOCK_ID "Stock-Id"
#define KEY_SELECTION "Selection"
#define KEY_EXTENSIONS "Extensions"
#define KEY_MIME_TYPES "Mimetypes"
#define KEY_SEPARATOR "Separator"
#define KEY_QUOTE_TYPE "Quote"
#define KEY_DEPENDENCIES "Dependencies"
#define KEY_CONDITIONS "Conditions"
#define KEY_WHITESPACE "EscapeSpaces"
#define KEY_DOUBLE_ESCAPE_QUOTES "DoubleEscapeQuotes"
#define KEY_TERMINAL "Terminal"
#define KEY_URI_SCHEME "UriScheme"
#define KEY_FILES "Files"
#define KEY_LOCATIONS "Locations"

#endif // NEMO_ACTION_SYMBOLS_H
