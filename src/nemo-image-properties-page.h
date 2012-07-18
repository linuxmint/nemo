/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef NEMO_IMAGE_PROPERTIES_PAGE_H
#define NEMO_IMAGE_PROPERTIES_PAGE_H

#include <gtk/gtk.h>

#define NEMO_TYPE_IMAGE_PROPERTIES_PAGE nemo_image_properties_page_get_type()
#define NEMO_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_IMAGE_PROPERTIES_PAGE, NemoImagePropertiesPage))
#define NEMO_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_IMAGE_PROPERTIES_PAGE, NemoImagePropertiesPageClass))
#define NEMO_IS_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_IMAGE_PROPERTIES_PAGE))
#define NEMO_IS_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NEMO_TYPE_IMAGE_PROPERTIES_PAGE))
#define NEMO_IMAGE_PROPERTIES_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NEMO_TYPE_IMAGE_PROPERTIES_PAGE, NemoImagePropertiesPageClass))

typedef struct NemoImagePropertiesPageDetails NemoImagePropertiesPageDetails;

typedef struct {
	GtkBox parent;
	NemoImagePropertiesPageDetails *details;
} NemoImagePropertiesPage;

typedef struct {
	GtkBoxClass parent;
} NemoImagePropertiesPageClass;

GType nemo_image_properties_page_get_type (void);
void  nemo_image_properties_page_register (void);

#endif /* NEMO_IMAGE_PROPERTIES_PAGE_H */
