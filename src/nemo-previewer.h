/*
 * nemo-previewer: nemo previewer DBus wrapper
 *
 * Copyright (C) 2011, Red Hat, Inc.
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

#ifndef __NEMO_PREVIEWER_H__
#define __NEMO_PREVIEWER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PREVIEWER nemo_previewer_get_type()
#define NEMO_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PREVIEWER, NemoPreviewer))
#define NEMO_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PREVIEWER, NemoPreviewerClass))
#define NEMO_IS_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PREVIEWER))
#define NEMO_IS_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_PREVIEWER))
#define NEMO_PREVIEWER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_PREVIEWER, NemoPreviewerClass))

typedef struct _NemoPreviewerPriv NemoPreviewerPriv;

typedef struct {
  GObject parent;

  /* private */
  NemoPreviewerPriv *priv;
} NemoPreviewer;

typedef struct {
  GObjectClass parent_class;
} NemoPreviewerClass;

GType nemo_previewer_get_type (void);

NemoPreviewer *nemo_previewer_get_singleton (void);
void nemo_previewer_call_show_file (NemoPreviewer *previewer,
                                        const gchar *uri,
                                        guint xid,
					gboolean close_if_already_visible);
void nemo_previewer_call_close (NemoPreviewer *previewer);

G_END_DECLS

#endif /* __NEMO_PREVIEWER_H__ */
