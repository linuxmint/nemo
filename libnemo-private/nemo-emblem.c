/* nemo-emblem.c - Custom emblem with size and position control
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

#include <config.h>
#include "nemo-emblem.h"

struct _NemoEmblem {
    GObject parent_instance;

    GIcon *icon;
    NemoEmblemSize size;
    NemoEmblemPosition position;
};

static void nemo_emblem_icon_iface_init (GIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (NemoEmblem, nemo_emblem, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON, nemo_emblem_icon_iface_init))

static void
nemo_emblem_finalize (GObject *object)
{
    NemoEmblem *emblem = NEMO_EMBLEM (object);

    g_clear_object (&emblem->icon);

    G_OBJECT_CLASS (nemo_emblem_parent_class)->finalize (object);
}

static guint
nemo_emblem_hash (GIcon *icon)
{
    NemoEmblem *emblem = NEMO_EMBLEM (icon);

    return g_icon_hash (emblem->icon) ^ (emblem->size << 8) ^ (emblem->position << 16);
}

static gboolean
nemo_emblem_equal (GIcon *icon1, GIcon *icon2)
{
    NemoEmblem *a = NEMO_EMBLEM (icon1);
    NemoEmblem *b = NEMO_EMBLEM (icon2);

    return g_icon_equal (a->icon, b->icon) &&
           a->size == b->size &&
           a->position == b->position;
}

static GVariant *
nemo_emblem_serialize (GIcon *icon)
{
    NemoEmblem *emblem = NEMO_EMBLEM (icon);
    GVariant *inner;

    inner = g_icon_serialize (emblem->icon);
    if (inner == NULL) {
        return NULL;
    }

    GVariant *result = g_variant_new ("(vii)", inner, (gint32) emblem->size, (gint32) emblem->position);
    return result;
}

static void
nemo_emblem_icon_iface_init (GIconIface *iface)
{
    iface->hash = nemo_emblem_hash;
    iface->equal = nemo_emblem_equal;
    iface->serialize = nemo_emblem_serialize;
}

static void
nemo_emblem_class_init (NemoEmblemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nemo_emblem_finalize;
}

static void
nemo_emblem_init (NemoEmblem *emblem)
{
}

NemoEmblem *
nemo_emblem_new (GIcon              *icon,
                 NemoEmblemSize      size,
                 NemoEmblemPosition  position)
{
    NemoEmblem *emblem;

    g_return_val_if_fail (G_IS_ICON (icon), NULL);

    emblem = g_object_new (NEMO_TYPE_EMBLEM, NULL);
    emblem->icon = g_object_ref (icon);
    emblem->size = size;
    emblem->position = position;

    return emblem;
}

GIcon *
nemo_emblem_get_icon (NemoEmblem *emblem)
{
    g_return_val_if_fail (NEMO_IS_EMBLEM (emblem), NULL);

    return emblem->icon;
}

NemoEmblemSize
nemo_emblem_get_size (NemoEmblem *emblem)
{
    g_return_val_if_fail (NEMO_IS_EMBLEM (emblem), NEMO_EMBLEM_SIZE_SMALL);

    return emblem->size;
}

NemoEmblemPosition
nemo_emblem_get_position (NemoEmblem *emblem)
{
    g_return_val_if_fail (NEMO_IS_EMBLEM (emblem), NEMO_EMBLEM_POSITION_DEFAULT);

    return emblem->position;
}
