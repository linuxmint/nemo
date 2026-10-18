/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#ifndef NEMO_ACTION_WIZARD_H
#define NEMO_ACTION_WIZARD_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_ACTION_WIZARD (nemo_action_wizard_get_type ())
G_DECLARE_FINAL_TYPE (NemoActionWizard, nemo_action_wizard, NEMO, ACTION_WIZARD, GtkAssistant)

GtkWidget *nemo_action_wizard_new          (GtkWindow   *parent);
GtkWidget *nemo_action_wizard_new_for_file (GtkWindow   *parent,
                                            const gchar *action_path);

G_END_DECLS

#endif /* NEMO_ACTION_WIZARD_H */
