/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  nautilus-window-types: typedefs for window-related types.
 *
 *  Copyright (C) 1999, 2000, 2010 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef __NAUTILUS_WINDOW_TYPES_H__
#define __NAUTILUS_WINDOW_TYPES_H__

typedef struct _NautilusWindowPane NautilusWindowPane;
typedef struct _NautilusWindowPaneClass NautilusWindowPaneClass;

typedef struct NautilusWindow NautilusWindow;

typedef struct NautilusWindowSlot NautilusWindowSlot;
typedef struct NautilusWindowSlotClass NautilusWindowSlotClass;

typedef void (* NautilusWindowGoToCallback) (NautilusWindow *window,
                                             GError *error,
                                             gpointer user_data);

typedef enum {
        NAUTILUS_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1<<0,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW = 1<<1,
        NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB = 1<<2
} NautilusWindowOpenFlags;

#endif /* __NAUTILUS_WINDOW_TYPES_H__ */
