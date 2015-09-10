/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __NEMO_THUMBNAIL_PROBLEM_BAR_H
#define __NEMO_THUMBNAIL_PROBLEM_BAR_H

#include "nemo-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_THUMBNAIL_PROBLEM_BAR         (nemo_thumbnail_problem_bar_get_type ())
#define NEMO_THUMBNAIL_PROBLEM_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_THUMBNAIL_PROBLEM_BAR, NemoThumbnailProblemBar))
#define NEMO_THUMBNAIL_PROBLEM_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_THUMBNAIL_PROBLEM_BAR, NemoThumbnailProblemBarClass))
#define NEMO_IS_THUMBNAIL_PROBLEM_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_THUMBNAIL_PROBLEM_BAR))
#define NEMO_IS_THUMBNAIL_PROBLEM_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_THUMBNAIL_PROBLEM_BAR))
#define NEMO_THUMBNAIL_PROBLEM_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_THUMBNAIL_PROBLEM_BAR, NemoThumbnailProblemBarClass))

typedef struct NemoThumbnailProblemBarPrivate NemoThumbnailProblemBarPrivate;

typedef struct
{
	GtkInfoBar parent;

	NemoThumbnailProblemBarPrivate *priv;
} NemoThumbnailProblemBar;

typedef struct
{
	GtkInfoBarClass parent_class;
} NemoThumbnailProblemBarClass;

GType		 nemo_thumbnail_problem_bar_get_type	(void) G_GNUC_CONST;
GtkWidget       *nemo_thumbnail_problem_bar_new         (NemoView *view);

G_END_DECLS

#endif /* __NEMO_THUMBNAIL_PROBLEM_BAR_H */
