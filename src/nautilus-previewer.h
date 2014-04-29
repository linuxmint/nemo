/*
 * nautilus-previewer: nautilus previewer DBus wrapper
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_PREVIEWER_H__
#define __NAUTILUS_PREVIEWER_H__

#include <glib.h>

G_BEGIN_DECLS

void nautilus_previewer_call_show_file (const gchar *uri,
                                        guint        xid,
					gboolean     close_if_already_visible);
void nautilus_previewer_call_close     (void);

G_END_DECLS

#endif /* __NAUTILUS_PREVIEWER_H__ */
