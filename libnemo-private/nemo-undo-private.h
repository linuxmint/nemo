/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* xxx
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NEMO_UNDO_PRIVATE_H
#define NEMO_UNDO_PRIVATE_H

#include <libnemo-private/nemo-undo.h>
#include <libnemo-private/nemo-undo-manager.h>
#include <glib-object.h>

NemoUndoManager * nemo_undo_get_undo_manager    (GObject               *attached_object);
void                  nemo_undo_attach_undo_manager (GObject               *object,
							 NemoUndoManager   *manager);

#endif /* NEMO_UNDO_PRIVATE_H */
