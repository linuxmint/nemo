/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nemo
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nemo is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nemo-signaller.h: Class to manage nemo-wide signals that don't
 * correspond to any particular object.
 */

#ifndef NEMO_SIGNALLER_H
#define NEMO_SIGNALLER_H

#include <glib-object.h>

/* NemoSignaller is a class that manages signals between
   disconnected Nemo code. Nemo objects connect to these signals
   so that other objects can cause them to be emitted later, without
   the connecting and emit-causing objects needing to know about each
   other. It seems a shame to have to invent a subclass and a special
   object just for this purpose. Perhaps there's a better way to do 
   this kind of thing.
*/

/* Get the one and only NemoSignaller to connect with or emit signals for */
GObject *nemo_signaller_get_current (void);

#endif /* NEMO_SIGNALLER_H */
