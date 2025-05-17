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

typedef enum
{
    NEMO_TERMINAL_SYNC_NONE,
    NEMO_TERMINAL_SYNC_FM_TO_TERM,
    NEMO_TERMINAL_SYNC_TERM_TO_FM,
    NEMO_TERMINAL_SYNC_BOTH
} NemoTerminalSyncMode;

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
typedef struct _NemoWindowPane NemoWindowPane;

struct _NemoTerminalWidget
{
    GtkBox parent_instance;

    GtkWidget *scrolled_window;
    VteTerminal *terminal;
    GtkWidget *ssh_indicator;
    GtkWidget *container_paned;
    NemoWindowPane *pane;

    GSimpleActionGroup *action_group;

    gboolean is_visible;
    gboolean maintain_focus;
    gboolean in_toggling;
    gboolean needs_respawn;
    gboolean is_exiting_ssh;
    gboolean ssh_connecting;
    gboolean ignore_next_terminal_cd_signal;

    int height;
    guint focus_timeout_id;

    GFile *current_location;

    gchar *color_scheme;

    gboolean in_ssh_mode;
    NemoTerminalSyncMode ssh_sync_mode;
    NemoTerminalSyncMode pending_ssh_sync_mode;
    NemoTerminalSshAutoConnectMode ssh_auto_connect_mode;
    gchar *ssh_hostname;
    gchar *ssh_username;
    gchar *ssh_port;
    gchar *ssh_remote_path;

    NemoTerminalSyncMode local_sync_mode;
};

struct _NemoTerminalWidgetClass
{
    GtkBoxClass parent_class;
};

GType nemo_terminal_widget_get_type(void);

NemoTerminalWidget *nemo_terminal_widget_new(void);
NemoTerminalWidget *nemo_terminal_widget_new_with_location(GFile *location);

void spawn_terminal_in_widget(NemoTerminalWidget *self);
void nemo_terminal_widget_set_current_location(NemoTerminalWidget *self, GFile *location);
void nemo_terminal_widget_ensure_terminal_focus(NemoTerminalWidget *self);

gboolean nemo_terminal_widget_initialize_in_paned(NemoTerminalWidget *self,
                                                  GtkWidget *unused_view_content,
                                                  GtkWidget *view_overlay);

void nemo_terminal_widget_toggle_visible(NemoTerminalWidget *self);
void nemo_terminal_widget_toggle_visible_with_save(NemoTerminalWidget *self,
                                                   gboolean is_manual_toggle);
gboolean nemo_terminal_widget_get_visible(NemoTerminalWidget *self);
void nemo_terminal_widget_ensure_state(NemoTerminalWidget *self);

void nemo_terminal_widget_apply_new_size(NemoTerminalWidget *self);
int nemo_terminal_widget_get_default_height(void);
void nemo_terminal_widget_save_height(NemoTerminalWidget *self, int height);

const gchar *nemo_terminal_widget_get_color_scheme(NemoTerminalWidget *self);
void nemo_terminal_widget_set_color_scheme(NemoTerminalWidget *self, const gchar *scheme);
void nemo_terminal_widget_apply_color_scheme(NemoTerminalWidget *self);

G_END_DECLS

#endif