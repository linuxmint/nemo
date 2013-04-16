/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nemo-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id: nemo-notebook.h 8210 2008-04-11 20:05:25Z chpe $
 */

#ifndef NEMO_NOTEBOOK_H
#define NEMO_NOTEBOOK_H

#include <glib.h>
#include <gtk/gtk.h>
#include "nemo-window-slot.h"

G_BEGIN_DECLS

#define NEMO_TYPE_NOTEBOOK		(nemo_notebook_get_type ())
#define NEMO_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_NOTEBOOK, NemoNotebook))
#define NEMO_NOTEBOOK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), NEMO_TYPE_NOTEBOOK, NemoNotebookClass))
#define NEMO_IS_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), NEMO_TYPE_NOTEBOOK))
#define NEMO_IS_NOTEBOOK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), NEMO_TYPE_NOTEBOOK))
#define NEMO_NOTEBOOK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), NEMO_TYPE_NOTEBOOK, NemoNotebookClass))

typedef struct _NemoNotebookClass	NemoNotebookClass;
typedef struct _NemoNotebook		NemoNotebook;

struct _NemoNotebook
{
	GtkNotebook parent;
};

struct _NemoNotebookClass
{
        GtkNotebookClass parent_class;

	/* Signals */
	void	 (* tab_close_request)  (NemoNotebook *notebook,
					 NemoWindowSlot *slot);
};

GType		nemo_notebook_get_type		(void);

int		nemo_notebook_add_tab	(NemoNotebook *nb,
						 NemoWindowSlot *slot,
						 int position,
						 gboolean jump_to);
	
void		nemo_notebook_set_show_tabs	(NemoNotebook *nb,
						 gboolean show_tabs);

void		nemo_notebook_set_dnd_enabled (NemoNotebook *nb,
						   gboolean enabled);
void		nemo_notebook_sync_tab_label (NemoNotebook *nb,
						  NemoWindowSlot *slot);
void		nemo_notebook_sync_loading   (NemoNotebook *nb,
						  NemoWindowSlot *slot);

void		nemo_notebook_reorder_current_child_relative (NemoNotebook *notebook,
								  int offset);
void		nemo_notebook_set_current_page_relative (NemoNotebook *notebook,
							     int offset);

gboolean        nemo_notebook_can_reorder_current_child_relative (NemoNotebook *notebook,
								      int offset);
gboolean        nemo_notebook_can_set_current_page_relative (NemoNotebook *notebook,
								 int offset);

G_END_DECLS

#endif /* NEMO_NOTEBOOK_H */

