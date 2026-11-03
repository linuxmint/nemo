/* nemo-emblem.h - Custom emblem with size and position control
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

#ifndef NEMO_EMBLEM_H
#define NEMO_EMBLEM_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
    NEMO_EMBLEM_SIZE_EXTRA_SMALL,  /* ~15% of icon size */
    NEMO_EMBLEM_SIZE_SMALL,        /* ~25% of icon size */
    NEMO_EMBLEM_SIZE_MEDIUM,       /* ~33% of icon size */
    NEMO_EMBLEM_SIZE_LARGE         /* ~50% of icon size */
} NemoEmblemSize;

typedef enum {
    NEMO_EMBLEM_POSITION_DEFAULT,     /* auto-assign: cycles LR -> LL -> UR -> UL */
    NEMO_EMBLEM_POSITION_UPPER_LEFT,
    NEMO_EMBLEM_POSITION_UPPER_RIGHT,
    NEMO_EMBLEM_POSITION_LOWER_LEFT,
    NEMO_EMBLEM_POSITION_LOWER_RIGHT
} NemoEmblemPosition;

#define NEMO_TYPE_EMBLEM (nemo_emblem_get_type ())

G_DECLARE_FINAL_TYPE (NemoEmblem, nemo_emblem, NEMO, EMBLEM, GObject)

NemoEmblem *       nemo_emblem_new          (GIcon              *icon,
                                              NemoEmblemSize      size,
                                              NemoEmblemPosition  position);
GIcon *             nemo_emblem_get_icon     (NemoEmblem         *emblem);
NemoEmblemSize      nemo_emblem_get_size     (NemoEmblem         *emblem);
NemoEmblemPosition  nemo_emblem_get_position (NemoEmblem         *emblem);

G_END_DECLS

#endif /* NEMO_EMBLEM_H */
