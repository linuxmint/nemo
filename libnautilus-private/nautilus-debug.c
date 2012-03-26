/*
 * nautilus-debug: debug loggers for nautilus
 *
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Based on Empathy's empathy-debug.
 */

#include <stdarg.h>

#include <glib.h>

#include "nautilus-debug.h"

#include "nautilus-file.h"

#ifdef ENABLE_DEBUG

static DebugFlags flags = 0;
static gboolean initialized = FALSE;

static GDebugKey keys[] = {
  { "Application", NAUTILUS_DEBUG_APPLICATION },
  { "Bookmarks", NAUTILUS_DEBUG_BOOKMARKS },
  { "DBus", NAUTILUS_DEBUG_DBUS },
  { "DirectoryView", NAUTILUS_DEBUG_DIRECTORY_VIEW },
  { "File", NAUTILUS_DEBUG_FILE },
  { "IconContainer", NAUTILUS_DEBUG_ICON_CONTAINER },
  { "IconView", NAUTILUS_DEBUG_ICON_VIEW },
  { "ListView", NAUTILUS_DEBUG_LIST_VIEW },
  { "Mime", NAUTILUS_DEBUG_MIME },
  { "Places", NAUTILUS_DEBUG_PLACES },
  { "Previewer", NAUTILUS_DEBUG_PREVIEWER },
  { "Smclient", NAUTILUS_DEBUG_SMCLIENT },
  { "Window", NAUTILUS_DEBUG_WINDOW },
  { "Undo", NAUTILUS_DEBUG_UNDO },
  { 0, }
};

static void
nautilus_debug_set_flags_from_env ()
{
  guint nkeys;
  const gchar *flags_string;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  flags_string = g_getenv ("NAUTILUS_DEBUG");

  if (flags_string)
    nautilus_debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));

  initialized = TRUE;
}

void
nautilus_debug_set_flags (DebugFlags new_flags)
{
  flags |= new_flags;
  initialized = TRUE;
}

gboolean
nautilus_debug_flag_is_set (DebugFlags flag)
{
  return flag & flags;
}

void
nautilus_debug (DebugFlags flag,
                const gchar *format,
                ...)
{
  va_list args;
  va_start (args, format);
  nautilus_debug_valist (flag, format, args);
  va_end (args);
}

void
nautilus_debug_valist (DebugFlags flag,
                       const gchar *format,
                       va_list args)
{
  if (G_UNLIKELY(!initialized))
    nautilus_debug_set_flags_from_env ();

  if (flag & flags)
    g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

static void
nautilus_debug_files_valist (DebugFlags flag,
                             GList *files,
                             const gchar *format,
                             va_list args)
{
  NautilusFile *file;
  GList *l;
  gchar *uri, *msg;

  if (G_UNLIKELY (!initialized))
    nautilus_debug_set_flags_from_env ();

  if (!(flag & flags))
    return;

  msg = g_strdup_vprintf (format, args);

  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s:", msg);

  for (l = files; l != NULL; l = l->next)
    {
      file = l->data;
      uri = nautilus_file_get_uri (file);

      if (nautilus_file_is_gone (file)) {
        gchar *new_uri;

        /* Hack: this will create an invalid URI, but it's for
         * display purposes only.
         */
        new_uri = g_strconcat (uri ? uri : "", " (gone)", NULL);
        g_free (uri);
        uri = new_uri;
      }

      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "   %s", uri);
      g_free (uri);
    }

  g_free (msg);
}

void
nautilus_debug_files (DebugFlags flag,
                      GList *files,
                      const gchar *format,
                      ...)
{
  va_list args;

  va_start (args, format);
  nautilus_debug_files_valist (flag, files, format, args);
  va_end (args);
}

#endif /* ENABLE_DEBUG */
