/* nemo-emblemed-icon.c - Icon with custom emblem compositing
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
#include "nemo-emblemed-icon.h"

struct _NemoEmblemedIcon {
    GObject parent_instance;

    GIcon *icon;
    GList *emblems;
};

static void nemo_emblemed_icon_icon_iface_init (GIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (NemoEmblemedIcon, nemo_emblemed_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON, nemo_emblemed_icon_icon_iface_init))

static void
nemo_emblemed_icon_finalize (GObject *object)
{
    NemoEmblemedIcon *self = NEMO_EMBLEMED_ICON (object);

    g_clear_object (&self->icon);
    g_list_free_full (self->emblems, g_object_unref);

    G_OBJECT_CLASS (nemo_emblemed_icon_parent_class)->finalize (object);
}

static guint
nemo_emblemed_icon_hash (GIcon *icon)
{
    NemoEmblemedIcon *self = NEMO_EMBLEMED_ICON (icon);
    guint hash;
    GList *l;

    hash = g_icon_hash (self->icon);

    for (l = self->emblems; l != NULL; l = l->next) {
        hash ^= g_icon_hash (G_ICON (l->data));
    }

    return hash;
}

static gboolean
nemo_emblemed_icon_equal (GIcon *icon1, GIcon *icon2)
{
    NemoEmblemedIcon *a = NEMO_EMBLEMED_ICON (icon1);
    NemoEmblemedIcon *b = NEMO_EMBLEMED_ICON (icon2);
    GList *la, *lb;

    if (!g_icon_equal (a->icon, b->icon)) {
        return FALSE;
    }

    if (g_list_length (a->emblems) != g_list_length (b->emblems)) {
        return FALSE;
    }

    for (la = a->emblems, lb = b->emblems; la != NULL; la = la->next, lb = lb->next) {
        if (!g_icon_equal (G_ICON (la->data), G_ICON (lb->data))) {
            return FALSE;
        }
    }

    return TRUE;
}

static GVariant *
nemo_emblemed_icon_serialize (GIcon *icon)
{
    NemoEmblemedIcon *self = NEMO_EMBLEMED_ICON (icon);
    GVariantBuilder builder;
    GVariant *inner;
    GList *l;

    inner = g_icon_serialize (self->icon);
    if (inner == NULL) {
        return NULL;
    }

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

    for (l = self->emblems; l != NULL; l = l->next) {
        GVariant *emblem_v = g_icon_serialize (G_ICON (l->data));
        if (emblem_v != NULL) {
            g_variant_builder_add (&builder, "v", emblem_v);
        }
    }

    return g_variant_new ("(vav)", inner, &builder);
}

static void
nemo_emblemed_icon_icon_iface_init (GIconIface *iface)
{
    iface->hash = nemo_emblemed_icon_hash;
    iface->equal = nemo_emblemed_icon_equal;
    iface->serialize = nemo_emblemed_icon_serialize;
}

static void
nemo_emblemed_icon_class_init (NemoEmblemedIconClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nemo_emblemed_icon_finalize;
}

static void
nemo_emblemed_icon_init (NemoEmblemedIcon *self)
{
}

GIcon *
nemo_emblemed_icon_new (GIcon      *icon,
                        NemoEmblem *emblem)
{
    NemoEmblemedIcon *self;

    g_return_val_if_fail (G_IS_ICON (icon), NULL);

    self = g_object_new (NEMO_TYPE_EMBLEMED_ICON, NULL);
    self->icon = g_object_ref (icon);

    if (emblem != NULL) {
        self->emblems = g_list_append (self->emblems, g_object_ref (emblem));
    }

    return G_ICON (self);
}

void
nemo_emblemed_icon_add_emblem (NemoEmblemedIcon *emblemed_icon,
                               NemoEmblem       *emblem)
{
    g_return_if_fail (NEMO_IS_EMBLEMED_ICON (emblemed_icon));
    g_return_if_fail (NEMO_IS_EMBLEM (emblem));

    emblemed_icon->emblems = g_list_append (emblemed_icon->emblems,
                                            g_object_ref (emblem));
}

GIcon *
nemo_emblemed_icon_get_icon (NemoEmblemedIcon *emblemed_icon)
{
    g_return_val_if_fail (NEMO_IS_EMBLEMED_ICON (emblemed_icon), NULL);

    return emblemed_icon->icon;
}

GList *
nemo_emblemed_icon_get_emblems (NemoEmblemedIcon *emblemed_icon)
{
    g_return_val_if_fail (NEMO_IS_EMBLEMED_ICON (emblemed_icon), NULL);

    return emblemed_icon->emblems;
}
