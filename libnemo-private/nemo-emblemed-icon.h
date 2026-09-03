/* nemo-emblemed-icon.h - Icon with custom emblem compositing
 *
 * Copyright (C) 2026 Linux Mint
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
 */

#ifndef NEMO_EMBLEMED_ICON_H
#define NEMO_EMBLEMED_ICON_H

#include <gio/gio.h>
#include "nemo-emblem.h"

G_BEGIN_DECLS

#define NEMO_TYPE_EMBLEMED_ICON (nemo_emblemed_icon_get_type ())

G_DECLARE_FINAL_TYPE (NemoEmblemedIcon, nemo_emblemed_icon, NEMO, EMBLEMED_ICON, GObject)

GIcon *  nemo_emblemed_icon_new        (GIcon              *icon,
                                         NemoEmblem         *emblem);
void     nemo_emblemed_icon_add_emblem (NemoEmblemedIcon   *emblemed_icon,
                                         NemoEmblem         *emblem);
GIcon *  nemo_emblemed_icon_get_icon   (NemoEmblemedIcon   *emblemed_icon);
GList *  nemo_emblemed_icon_get_emblems (NemoEmblemedIcon  *emblemed_icon);

G_END_DECLS

#endif /* NEMO_EMBLEMED_ICON_H */
