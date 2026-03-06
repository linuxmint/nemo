/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * nemo-overview.h - Disk usage overview page with donut charts.
 *
 * Copyright (C) 2026 Nemo contributors
 *
 * Nemo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NEMO_OVERVIEW_H
#define NEMO_OVERVIEW_H

#include <gtk/gtk.h>

#define NEMO_TYPE_OVERVIEW (nemo_overview_get_type ())
G_DECLARE_FINAL_TYPE (NemoOverview, nemo_overview, NEMO, OVERVIEW, GtkScrolledWindow)

GtkWidget *nemo_overview_new (void);
void       nemo_overview_refresh (NemoOverview *self);

#endif /* NEMO_OVERVIEW_H */
