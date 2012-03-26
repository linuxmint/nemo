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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_PREVIEWER_H__
#define __NAUTILUS_PREVIEWER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PREVIEWER nautilus_previewer_get_type()
#define NAUTILUS_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PREVIEWER, NautilusPreviewer))
#define NAUTILUS_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PREVIEWER, NautilusPreviewerClass))
#define NAUTILUS_IS_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PREVIEWER))
#define NAUTILUS_IS_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PREVIEWER))
#define NAUTILUS_PREVIEWER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PREVIEWER, NautilusPreviewerClass))

typedef struct _NautilusPreviewerPriv NautilusPreviewerPriv;

typedef struct {
  GObject parent;

  /* private */
  NautilusPreviewerPriv *priv;
} NautilusPreviewer;

typedef struct {
  GObjectClass parent_class;
} NautilusPreviewerClass;

GType nautilus_previewer_get_type (void);

NautilusPreviewer *nautilus_previewer_get_singleton (void);
void nautilus_previewer_call_show_file (NautilusPreviewer *previewer,
                                        const gchar *uri,
                                        guint xid,
					gboolean close_if_already_visible);
void nautilus_previewer_call_close (NautilusPreviewer *previewer);

G_END_DECLS

#endif /* __NAUTILUS_PREVIEWER_H__ */
