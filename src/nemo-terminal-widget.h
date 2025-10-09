/* nemo-terminal-widget.h

  Copyright (C) 2025

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.

  Author: Bruno Goncalves <biglinux.com.br>
 */

#ifndef __NEMO_TERMINAL_WIDGET_H__
#define __NEMO_TERMINAL_WIDGET_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <vte/vte.h>

G_BEGIN_DECLS

/**
 * NemoTerminalSyncMode:
 * @NEMO_TERMINAL_SYNC_NONE: No synchronization between file manager and terminal.
 * @NEMO_TERMINAL_SYNC_FM_TO_TERM: File manager navigation changes the terminal's directory.
 * @NEMO_TERMINAL_SYNC_TERM_TO_FM: Terminal `cd` commands change the file manager's location.
 * @NEMO_TERMINAL_SYNC_BOTH: Synchronization is bidirectional.
 *
 * Defines the synchronization behavior for the terminal's current directory.
 */
typedef enum
{
    NEMO_TERMINAL_SYNC_NONE,
    NEMO_TERMINAL_SYNC_FM_TO_TERM,
    NEMO_TERMINAL_SYNC_TERM_TO_FM,
    NEMO_TERMINAL_SYNC_BOTH
} NemoTerminalSyncMode;

/**
 * NemoTerminalSshAutoConnectMode:
 * @NEMO_TERMINAL_SSH_AUTOCONNECT_OFF: Do not automatically connect to SSH when navigating to an SFTP location.
 * @NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH: Automatically connect and sync both ways.
 * @NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM: Automatically connect and sync from file manager to terminal.
 * @NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM: Automatically connect and sync from terminal to file manager.
 * @NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE: Automatically connect but do not sync directories.
 *
 * Defines the auto-connection behavior when the file manager navigates to an SFTP location.
 */
typedef enum
{
    NEMO_TERMINAL_SSH_AUTOCONNECT_OFF,
    NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH,
    NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM,
    NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM,
    NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE
} NemoTerminalSshAutoConnectMode;

#define NEMO_TYPE_TERMINAL_WIDGET (nemo_terminal_widget_get_type())
#define NEMO_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NEMO_TYPE_TERMINAL_WIDGET, NemoTerminalWidget))
#define NEMO_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), NEMO_TYPE_TERMINAL_WIDGET, NemoTerminalWidgetClass))
#define NEMO_IS_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), NEMO_TYPE_TERMINAL_WIDGET))
#define NEMO_IS_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), NEMO_TYPE_TERMINAL_WIDGET))
#define NEMO_TERMINAL_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), NEMO_TYPE_TERMINAL_WIDGET, NemoTerminalWidgetClass))

typedef struct _NemoTerminalWidget NemoTerminalWidget;
typedef struct _NemoTerminalWidgetClass NemoTerminalWidgetClass;
typedef struct _NemoTerminalWidgetPrivate NemoTerminalWidgetPrivate;

struct _NemoTerminalWidget
{
    GtkBox parent_instance;
    NemoTerminalWidgetPrivate *priv;
};

struct _NemoTerminalWidgetClass
{
    GtkBoxClass parent_class;
};

GType nemo_terminal_widget_get_type(void) G_GNUC_CONST;

NemoTerminalWidget *nemo_terminal_widget_new(void);
NemoTerminalWidget *nemo_terminal_widget_new_with_location(GFile *location);

void nemo_terminal_widget_set_current_location(NemoTerminalWidget *self,
                                               GFile *location);
void nemo_terminal_widget_set_container_paned(NemoTerminalWidget *self,
                                              GtkWidget *paned);

void nemo_terminal_widget_toggle_visible(NemoTerminalWidget *self);
void nemo_terminal_widget_ensure_state(NemoTerminalWidget *self);
void nemo_terminal_widget_ensure_terminal_focus(NemoTerminalWidget *self);

gboolean nemo_terminal_widget_get_visible(NemoTerminalWidget *self);

void nemo_terminal_widget_apply_new_size(NemoTerminalWidget *self);

G_END_DECLS

#endif /* __NEMO_TERMINAL_WIDGET_H__ */
