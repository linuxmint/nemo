/*
 * nemo-dbus-manager: nemo DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nemo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NEMO_DBUS_MANAGER_H__
#define __NEMO_DBUS_MANAGER_H__

#include <glib-object.h>
#include <gio/gio.h>

void nemo_dbus_manager_start (GApplication *application);
void nemo_dbus_manager_stop (void);

#endif /* __NEMO_DBUS_MANAGER_H__ */
