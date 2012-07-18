/*
 *  nemo-property-page.h - Property pages exported by 
 *                             NemoPropertyProvider objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#ifndef NEMO_PROPERTY_PAGE_H
#define NEMO_PROPERTY_PAGE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nemo-extension-types.h"

G_BEGIN_DECLS

#define NEMO_TYPE_PROPERTY_PAGE            (nemo_property_page_get_type())
#define NEMO_PROPERTY_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NEMO_TYPE_PROPERTY_PAGE, NemoPropertyPage))
#define NEMO_PROPERTY_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NEMO_TYPE_PROPERTY_PAGE, NemoPropertyPageClass))
#define NEMO_IS_PROPERTY_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NEMO_TYPE_PROPERTY_PAGE))
#define NEMO_IS_PROPERTY_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NEMO_TYPE_PROPERTY_PAGE))
#define NEMO_PROPERTY_PAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NEMO_TYPE_PROPERTY_PAGE, NemoPropertyPageClass))

typedef struct _NemoPropertyPage        NemoPropertyPage;
typedef struct _NemoPropertyPageDetails NemoPropertyPageDetails;
typedef struct _NemoPropertyPageClass   NemoPropertyPageClass;

struct _NemoPropertyPage
{
	GObject parent;

	NemoPropertyPageDetails *details;
};

struct _NemoPropertyPageClass 
{
	GObjectClass parent;
};

GType                 nemo_property_page_get_type  (void);
NemoPropertyPage *nemo_property_page_new       (const char           *name,
							GtkWidget            *label,
							GtkWidget            *page);

/* NemoPropertyPage has the following properties:
 *   name (string)        - the identifier for the property page
 *   label (widget)       - the user-visible label of the property page
 *   page (widget)        - the property page to display
 */

G_END_DECLS

#endif
