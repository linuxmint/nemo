/* nemo-terminal-widget.c

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

#include "nemo-terminal-widget.h"
#include "nemo-window-slot.h"
#include "nemo-global-preferences.h"

#include <glib/gi18n.h>
#include <vte/vte.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

/* UI constants */
#define MIN_MAIN_VIEW_HEIGHT 200
#define MIN_TERMINAL_HEIGHT 50
#define MIN_FONT_SIZE 6
#define MAX_FONT_SIZE 72

/* GObject properties */
enum
{
  PROP_0,
  PROP_CURRENT_LOCATION,
  N_PROPS
};

/* GObject signals */
enum
{
  CHANGE_DIRECTORY,
  TOGGLE_VISIBILITY,
  LAST_SIGNAL
};

/*
 * NemoTerminalState:
 * @NEMO_TERMINAL_STATE_LOCAL: The terminal is running a local shell.
 * @NEMO_TERMINAL_STATE_IN_SSH: The terminal is in an active SSH session.
 *
 * Represents the operational state of the terminal widget. This is crucial
 * for determining how directory synchronization and commands are handled.
 */
typedef enum
{
  NEMO_TERMINAL_STATE_LOCAL,
  NEMO_TERMINAL_STATE_IN_SSH,
} NemoTerminalState;

struct _NemoTerminalWidgetPrivate
{
  /* Child widgets */
  GtkWidget   *scrolled_window; /* The GtkScrolledWindow that makes the terminal scrollable. */
  VteTerminal *terminal;        /* The core VTE terminal widget. */
  GtkWidget   *ssh_indicator;   /* A label shown at the top to indicate an active SSH session. */

  /* State management */
  NemoTerminalState state;                          /* The current operational state (local shell or remote SSH session). */
  gboolean            is_visible;                     /* Tracks if the terminal widget is currently shown to the user. */
  gboolean            in_toggling;                    /* A re-entrancy guard for the visibility toggle function to prevent rapid, repeated calls. */
  gboolean            needs_respawn;                  /* Flag indicating if the terminal's child process needs to be respawned (e.g., after being hidden and shown again). */
  gboolean            ignore_next_terminal_cd_signal; /* A flag to prevent feedback loops when the file manager programmatically changes the terminal's directory. */
  guint               focus_timeout_id;               /* The ID for a timeout used to ensure the terminal gets focus after certain operations. */
  GPid                child_pid;                      /* The process ID of the shell or SSH client running in the terminal. -1 if no process is running. */
  GCancellable       *spawn_cancellable;              /* A GCancellable object to allow cancelling an asynchronous terminal spawn operation. */
  GWeakRef            paned_weak_ref;                 /* A weak reference to the parent GtkPaned to avoid circular references and allow size adjustments. */

  /* Preferences */
  gchar                *color_scheme;          /* The name of the current color scheme (e.g., "dark", "solarized-light"). */
  NemoTerminalSyncMode  local_sync_mode;       /* The directory synchronization mode for local shell sessions. */
  NemoTerminalSshAutoConnectMode ssh_auto_connect_mode; /* The auto-connection behavior when navigating to SFTP locations. */

  /* Location and SSH details */
  GFile *current_location; /* The GFile representing the current directory displayed in the file manager view. */

  gchar                 *ssh_hostname;    /* The hostname for the current SSH connection. */
  gchar                 *ssh_username;    /* The username for the current SSH connection. */
  gchar                 *ssh_port;        /* The port for the current SSH connection. */
  gchar                 *ssh_remote_path; /* The remote path to change to after an SSH connection is established. */
  NemoTerminalSyncMode   ssh_sync_mode;   /* The directory synchronization mode for the current SSH session. */

  /* Pending SSH connection details, used when a location is set before the terminal is spawned */
  gchar                 *pending_ssh_hostname;  /* The hostname for a pending SSH connection, to be used after the local shell spawns. */
  gchar                 *pending_ssh_username;  /* The username for a pending SSH connection. */
  gchar                 *pending_ssh_port;      /* The port for a pending SSH connection. */
  NemoTerminalSyncMode   pending_ssh_sync_mode; /* The sync mode for a pending SSH connection. */
};

/* Data keys for g_object_set_data() */
static const gchar *const DATA_KEY_SSH_HOSTNAME = "ntw-ssh-hostname";
static const gchar *const DATA_KEY_SSH_USERNAME = "ntw-ssh-username";
static const gchar *const DATA_KEY_SSH_PORT = "ntw-ssh-port";
static const gchar *const DATA_KEY_SSH_SYNC_MODE = "ntw-ssh-sync-mode";

/* Shell control sequences for preserving user input during programmatic 'cd' */
static const gchar *const SHELL_CTRL_A = "\x01";    /* Move cursor to beginning of line */
static const gchar *const SHELL_CTRL_K = "\x0B";    /* Kill (cut) from cursor to end of line */
static const gchar *const SHELL_CTRL_Y = "\x19";    /* Yank (paste) killed text */
static const gchar *const SHELL_CTRL_E = "\x05";    /* Move cursor to end of line */
static const gchar *const SHELL_DELETE = "\033[3~"; /* Delete character under cursor */

typedef struct
{
  GdkRGBA  foreground;
  GdkRGBA  background;
  GdkRGBA  palette[16];
  gboolean use_system_colors;
} NemoTerminalColorPalette;

#define RGB(r, g, b) ((GdkRGBA) { .red = (r), .green = (g), .blue = (b), .alpha = 1.0 })

typedef struct
{
  const gchar                     *id;
  const gchar                     *label_pot;
  const NemoTerminalColorPalette   palette;
} MenuSchemeEntry;

/* clang-format off */
static const MenuSchemeEntry COLOR_SCHEME_ENTRIES[] = {
  { "system", N_("System"), .palette = { .use_system_colors = TRUE } },
  { "dark", N_("Dark"), .palette = {
      .foreground = RGB(0.9, 0.9, 0.9), .background = RGB(0.12, 0.12, 0.12),
      .palette = {
        RGB(0.0, 0.0, 0.0), RGB(0.8, 0.0, 0.0), RGB(0.0, 0.8, 0.0), RGB(0.8, 0.8, 0.0),
        RGB(0.0, 0.0, 0.8), RGB(0.8, 0.0, 0.8), RGB(0.0, 0.8, 0.8), RGB(0.8, 0.8, 0.8),
        RGB(0.5, 0.5, 0.5), RGB(1.0, 0.4, 0.4), RGB(0.4, 1.0, 0.4), RGB(1.0, 1.0, 0.4),
        RGB(0.4, 0.4, 1.0), RGB(1.0, 0.4, 1.0), RGB(0.4, 1.0, 1.0), RGB(1.0, 1.0, 1.0)
      }
    }
  },
  { "light", N_("Light"), .palette = {
      .foreground = RGB(0.15, 0.15, 0.15), .background = RGB(0.98, 0.98, 0.98),
      .palette = {
        RGB(0.2, 0.2, 0.2), RGB(0.8, 0.2, 0.2), RGB(0.1, 0.6, 0.1), RGB(0.7, 0.6, 0.1),
        RGB(0.2, 0.4, 0.7), RGB(0.6, 0.3, 0.5), RGB(0.3, 0.6, 0.7), RGB(0.7, 0.7, 0.7),
        RGB(0.4, 0.4, 0.4), RGB(0.9, 0.3, 0.3), RGB(0.2, 0.7, 0.2), RGB(0.8, 0.7, 0.2),
        RGB(0.3, 0.5, 0.8), RGB(0.7, 0.4, 0.6), RGB(0.4, 0.7, 0.8), RGB(0.9, 0.9, 0.9)
      }
    }
  },
  { "solarized-dark", N_("Solarized Dark"), .palette = {
      .foreground = RGB(0.8235, 0.8588, 0.8706), .background = RGB(0.0000, 0.1686, 0.2118),
      .palette = {
        RGB(0.0275, 0.2118, 0.2588), RGB(0.8627, 0.1961, 0.1843), RGB(0.5216, 0.6000, 0.0000), RGB(0.7098, 0.5412, 0.0000),
        RGB(0.1490, 0.5451, 0.8235), RGB(0.8275, 0.2118, 0.5098), RGB(0.1647, 0.6314, 0.6000), RGB(0.9294, 0.9098, 0.8353),
        RGB(0.0000, 0.1686, 0.2118), RGB(0.8000, 0.2588, 0.2078), RGB(0.3725, 0.4235, 0.4314), RGB(0.4078, 0.4745, 0.4784),
        RGB(0.5137, 0.5804, 0.5843), RGB(0.4235, 0.4431, 0.6118), RGB(0.5804, 0.6078, 0.5373), RGB(0.9922, 0.9647, 0.8902)
      }
    }
  },
  { "solarized-light", N_("Solarized Light"), .palette = {
      .foreground = RGB(0.4000, 0.4784, 0.5098), .background = RGB(0.9922, 0.9647, 0.8902),
      .palette = {
        RGB(0.0275, 0.2118, 0.2588), RGB(0.8627, 0.1961, 0.1843), RGB(0.5216, 0.6000, 0.0000), RGB(0.7098, 0.5412, 0.0000),
        RGB(0.1490, 0.5451, 0.8235), RGB(0.8275, 0.2118, 0.5098), RGB(0.1647, 0.6314, 0.6000), RGB(0.9294, 0.9098, 0.8353),
        RGB(0.0000, 0.1686, 0.2118), RGB(0.8000, 0.2588, 0.2078), RGB(0.3725, 0.4235, 0.4314), RGB(0.4078, 0.4745, 0.4784),
        RGB(0.5137, 0.5804, 0.5843), RGB(0.4235, 0.4431, 0.6118), RGB(0.5804, 0.6078, 0.5373), RGB(0.8235, 0.8588, 0.8706)
      }
    }
  },
  { "matrix", N_("Matrix"), .palette = {
      .foreground = RGB(0.1, 0.9, 0.1), .background = RGB(0.0, 0.0, 0.0),
      .palette = {
        RGB(0.0, 0.0, 0.0), RGB(0.0, 0.5, 0.0), RGB(0.0, 0.8, 0.0), RGB(0.1, 0.6, 0.0),
        RGB(0.0, 0.4, 0.0), RGB(0.1, 0.5, 0.1), RGB(0.0, 0.7, 0.1), RGB(0.1, 0.9, 0.1),
        RGB(0.0, 0.3, 0.0), RGB(0.0, 0.6, 0.0), RGB(0.0, 1.0, 0.0), RGB(0.2, 0.7, 0.0),
        RGB(0.0, 0.5, 0.0), RGB(0.2, 0.6, 0.2), RGB(0.0, 0.8, 0.2), RGB(0.2, 1.0, 0.2)
      }
    }
  },
  { "one-half-dark", N_("One Half Dark"), .palette = {
      .foreground = RGB(0.870, 0.870, 0.870), .background = RGB(0.157, 0.168, 0.184),
      .palette = {
        RGB(0.157, 0.168, 0.184), RGB(0.882, 0.490, 0.470), RGB(0.560, 0.749, 0.450), RGB(0.941, 0.768, 0.470),
        RGB(0.400, 0.627, 0.850), RGB(0.768, 0.470, 0.800), RGB(0.341, 0.709, 0.729), RGB(0.870, 0.870, 0.870),
        RGB(0.400, 0.450, 0.500), RGB(0.882, 0.490, 0.470), RGB(0.560, 0.749, 0.450), RGB(0.941, 0.768, 0.470),
        RGB(0.400, 0.627, 0.850), RGB(0.768, 0.470, 0.800), RGB(0.341, 0.709, 0.729), RGB(0.970, 0.970, 0.970)
      }
    }
  },
  { "one-half-light", N_("One Half Light"), .palette = {
      .foreground = RGB(0.220, 0.240, 0.260), .background = RGB(0.980, 0.980, 0.980),
      .palette = {
        RGB(0.220, 0.240, 0.260), RGB(0.858, 0.200, 0.180), RGB(0.310, 0.600, 0.110), RGB(0.850, 0.588, 0.100),
        RGB(0.231, 0.490, 0.749), RGB(0.670, 0.270, 0.729), RGB(0.149, 0.639, 0.678), RGB(0.800, 0.800, 0.800),
        RGB(0.400, 0.400, 0.400), RGB(0.858, 0.200, 0.180), RGB(0.310, 0.600, 0.110), RGB(0.850, 0.588, 0.100),
        RGB(0.231, 0.490, 0.749), RGB(0.670, 0.270, 0.729), RGB(0.149, 0.639, 0.678), RGB(0.080, 0.080, 0.080)
      }
    }
  },
  { "monokai", N_("Monokai"), .palette = {
      .foreground = RGB(0.929, 0.925, 0.910), .background = RGB(0, 0, 0),
      .palette = {
        RGB(0.153, 0.157, 0.149), RGB(0.980, 0.149, 0.450), RGB(0.650, 0.890, 0.180), RGB(0.960, 0.780, 0.310),
        RGB(0.208, 0.580, 0.839), RGB(0.670, 0.380, 0.960), RGB(0.400, 0.950, 0.950), RGB(0.929, 0.925, 0.910),
        RGB(0.400, 0.400, 0.400), RGB(0.980, 0.149, 0.450), RGB(0.650, 0.890, 0.180), RGB(0.960, 0.780, 0.310),
        RGB(0.208, 0.580, 0.839), RGB(0.670, 0.380, 0.960), RGB(0.400, 0.950, 0.950), RGB(1.000, 1.000, 1.000)
      }
    }
  },
};
/* clang-format on */

typedef struct
{
  int size_pts;
} MenuFontSizeEntry;

static const MenuFontSizeEntry FONT_SIZE_ENTRIES[] = {
  { 9 }, { 10 }, { 11 }, { 12 }, { 13 }, { 14 }, { 15 }, { 16 },
  { 17 }, { 18 }, { 20 }, { 22 }, { 24 }, { 28 }, { 32 }, { 36 }, { 40 }, { 48 }
};

typedef struct
{
  NemoTerminalSyncMode mode;
  const gchar         *label_pot;
} MenuSyncModeEntry;

static const MenuSyncModeEntry LOCAL_SYNC_MODE_ENTRIES[] = {
  { NEMO_TERMINAL_SYNC_BOTH, N_("Sync Both Ways") },
  { NEMO_TERMINAL_SYNC_FM_TO_TERM, N_("Sync File Manager → Terminal") },
  { NEMO_TERMINAL_SYNC_TERM_TO_FM, N_("Sync Terminal → File Manager") },
  { NEMO_TERMINAL_SYNC_NONE, N_("No Sync") }
};

typedef struct
{
  NemoTerminalSshAutoConnectMode mode;
  const gchar                     *label_pot;
} MenuSshAutoConnectEntry;

static const MenuSshAutoConnectEntry SFTP_AUTO_CONNECT_ENTRIES[] = {
  { NEMO_TERMINAL_SSH_AUTOCONNECT_OFF, N_("Do not connect automatically") },
  { NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH, N_("Automatically connect and sync both ways") },
  { NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM, N_("Automatically connect and sync: File Manager → Terminal") },
  { NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM, N_("Automatically connect and sync: Terminal → File Manager") },
  { NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE, N_("Automatically connect without syncing") }
};

static const MenuSyncModeEntry MANUAL_SSH_SYNC_ENTRIES[] = {
  { NEMO_TERMINAL_SYNC_BOTH, N_("Sync folder both ways") },
  { NEMO_TERMINAL_SYNC_FM_TO_TERM, N_("Sync folder from File Manager → Terminal") },
  { NEMO_TERMINAL_SYNC_TERM_TO_FM, N_("Sync folder from Terminal → File Manager") },
  { NEMO_TERMINAL_SYNC_NONE, N_("No folder sync") }
};

static const gchar *const DATA_KEY_VALUE = "ntw-value";

/* Forward declarations */
static void spawn_terminal_async(NemoTerminalWidget *self);
static void on_terminal_child_exited(VteTerminal *terminal, gint status, gpointer user_data);
static gboolean on_terminal_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean on_terminal_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
static void on_terminal_directory_changed(VteTerminal *terminal, gpointer user_data);
static void on_color_scheme_changed(GtkCheckMenuItem *menuitem, gpointer user_data);
static void on_font_size_changed(GtkCheckMenuItem *menuitem, gpointer user_data);
static void on_enum_pref_changed(GtkCheckMenuItem *menuitem, gpointer user_data);
static void on_terminal_preference_changed(GSettings *settings, const gchar *key, gpointer user_data);
static void setup_terminal_font(VteTerminal *terminal);
static void nemo_terminal_widget_apply_color_scheme(NemoTerminalWidget *self);
static void _initiate_ssh_connection(NemoTerminalWidget *self, const gchar *hostname, const gchar *username, const gchar *port, NemoTerminalSyncMode sync_mode);
static gboolean parse_gvfs_ssh_path(GFile *location, gchar **hostname, gchar **username, gchar **port);
static void change_directory_in_terminal(NemoTerminalWidget *self, GFile *location);
static void _clear_ssh_connection_data(NemoTerminalWidgetPrivate *priv);
static void _reset_to_local_state(NemoTerminalWidget *self);
static const gchar * nemo_terminal_widget_get_color_scheme(NemoTerminalWidget *self);
static void nemo_terminal_widget_set_color_scheme(NemoTerminalWidget *self, const gchar *scheme);
static void _sync_terminal_to_fm (NemoTerminalWidget *self, const gchar *cwd_uri);

static GParamSpec *properties[N_PROPS];
static guint       signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE(NemoTerminalWidget, nemo_terminal_widget, GTK_TYPE_BOX,
                        G_ADD_PRIVATE(NemoTerminalWidget))

static void
nemo_terminal_widget_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(object);

    switch (prop_id)
    {
        case PROP_CURRENT_LOCATION:
            nemo_terminal_widget_set_current_location(self, g_value_get_object(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
nemo_terminal_widget_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(object);
    NemoTerminalWidgetPrivate *priv = self->priv;

    switch (prop_id)
    {
        case PROP_CURRENT_LOCATION:
            g_value_set_object(value, priv->current_location);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
nemo_terminal_widget_finalize(GObject *object)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(object);
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_autoptr(GtkWidget) paned = g_weak_ref_get(&priv->paned_weak_ref);

    g_signal_handlers_disconnect_by_data(nemo_window_state, self);

    if (paned)
    {
        g_signal_handlers_disconnect_by_data(paned, self);
    }
    g_weak_ref_clear(&priv->paned_weak_ref);

    if (priv->focus_timeout_id > 0)
        g_source_remove(priv->focus_timeout_id);

    g_cancellable_cancel(priv->spawn_cancellable);
    g_clear_object(&priv->spawn_cancellable);
    g_clear_object(&priv->current_location);
    g_clear_pointer(&priv->color_scheme, g_free);

    _clear_ssh_connection_data(priv);

    G_OBJECT_CLASS(nemo_terminal_widget_parent_class)->finalize(object);
}

static void
nemo_terminal_widget_class_init(NemoTerminalWidgetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = nemo_terminal_widget_set_property;
    object_class->get_property = nemo_terminal_widget_get_property;
    object_class->finalize = nemo_terminal_widget_finalize;

    properties[PROP_CURRENT_LOCATION] =
        g_param_spec_object("current-location",
                            "Current Location",
                            "The GFile representing the current directory.",
                            G_TYPE_FILE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties(object_class, N_PROPS, properties);

    signals[CHANGE_DIRECTORY] =
        g_signal_new("change-directory",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1, G_TYPE_FILE);

    signals[TOGGLE_VISIBILITY] =
        g_signal_new("toggle-visibility",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__BOOLEAN,
                     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void
nemo_terminal_widget_init(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = nemo_terminal_widget_get_instance_private(self);
    GtkStyleContext *context = NULL;
    g_autoptr(GtkCssProvider) provider = NULL;
    GtkWidget *vbox;

    self->priv = priv;

    priv->state = NEMO_TERMINAL_STATE_LOCAL;
    priv->needs_respawn = TRUE;
    priv->child_pid = -1;
    priv->spawn_cancellable = g_cancellable_new();
    g_weak_ref_init(&priv->paned_weak_ref, NULL);

    priv->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(priv->scrolled_window, TRUE);
    gtk_widget_set_hexpand(priv->scrolled_window, TRUE);

    priv->terminal = VTE_TERMINAL(vte_terminal_new());
    vte_terminal_set_scroll_on_output(priv->terminal, FALSE);
    vte_terminal_set_scroll_on_keystroke(priv->terminal, TRUE);
    vte_terminal_set_scrollback_lines(priv->terminal, 10000);
    gtk_widget_set_can_focus(GTK_WIDGET(priv->terminal), TRUE);

    priv->ssh_indicator = gtk_label_new("SSH");
    gtk_widget_set_name(priv->ssh_indicator, "ssh-indicator");
    gtk_widget_set_no_show_all(priv->ssh_indicator, TRUE);
    gtk_widget_hide(priv->ssh_indicator);
    gtk_widget_set_vexpand(priv->ssh_indicator, FALSE);
    gtk_widget_set_hexpand(priv->ssh_indicator, TRUE);
    gtk_label_set_xalign(GTK_LABEL(priv->ssh_indicator), 0.5);

    provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, "label#ssh-indicator { background-color: @theme_selected_bg_color; color: @theme_selected_fg_color; padding: 2px 5px; margin: 0; font-weight: bold; }", -1, NULL);
    context = gtk_widget_get_style_context(priv->ssh_indicator);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), priv->ssh_indicator, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(priv->scrolled_window), GTK_WIDGET(priv->terminal));
    gtk_box_pack_start(GTK_BOX(vbox), priv->scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(self), vbox, TRUE, TRUE, 0);

    priv->local_sync_mode = g_settings_get_enum(nemo_window_state, "local-terminal-sync-mode");
    priv->ssh_auto_connect_mode = g_settings_get_enum(nemo_window_state, "ssh-terminal-auto-connect-mode");

    setup_terminal_font(priv->terminal);
    nemo_terminal_widget_get_color_scheme(self);
    nemo_terminal_widget_apply_color_scheme(self);

    g_signal_connect(nemo_window_state, "changed", G_CALLBACK(on_terminal_preference_changed), self);

    g_signal_connect(priv->terminal, "child-exited", G_CALLBACK(on_terminal_child_exited), self);
    g_signal_connect(priv->terminal, "button-press-event", G_CALLBACK(on_terminal_button_press), self);
    g_signal_connect(priv->terminal, "key-press-event", G_CALLBACK(on_terminal_key_press), self);
    g_signal_connect(priv->terminal, "current-directory-uri-changed", G_CALLBACK(on_terminal_directory_changed), self);

    gtk_widget_show_all(GTK_WIDGET(self));
    gtk_widget_hide(GTK_WIDGET(self));
}

static void
spawn_async_callback(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;

    if (gtk_widget_in_destruction(GTK_WIDGET(self)))
    {
        return;
    }

    if (pid == -1)
    {
        g_warning("Failed to spawn terminal: %s", error ? error->message : "Unknown error");
        priv->needs_respawn = TRUE;
        priv->child_pid = -1;
    }
    else
    {
        priv->child_pid = pid;
        priv->needs_respawn = FALSE;
        if (priv->pending_ssh_hostname)
        {
            _initiate_ssh_connection(self, priv->pending_ssh_hostname, priv->pending_ssh_username, priv->pending_ssh_port, priv->pending_ssh_sync_mode);
            g_clear_pointer(&priv->pending_ssh_hostname, g_free);
            g_clear_pointer(&priv->pending_ssh_username, g_free);
            g_clear_pointer(&priv->pending_ssh_port, g_free);
        }
    }
}

static void
spawn_terminal_async(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_autofree gchar *working_directory = NULL;
    const gchar *shell_executable;
    gchar **argv = NULL;
    gchar **envp = NULL;

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    if (priv->child_pid != -1)
        return;

    if (g_cancellable_is_cancelled(priv->spawn_cancellable))
    {
        g_clear_object(&priv->spawn_cancellable);
        priv->spawn_cancellable = g_cancellable_new();
    }

    shell_executable = g_getenv("SHELL");
    if (!shell_executable || *shell_executable == '\0')
        shell_executable = "/bin/sh";

    if (priv->pending_ssh_hostname != NULL)
    {
        working_directory = NULL;
    }
    else if (priv->current_location)
    {
        g_autoptr(GFile) dir_location = NULL;
        g_autoptr(GFileInfo) info = g_file_query_info(priv->current_location, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
        if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
            dir_location = g_object_ref(priv->current_location);
        } else {
            dir_location = g_file_get_parent(priv->current_location);
        }
        if (dir_location) {
            working_directory = g_file_get_path(dir_location);
        }
    }

    if (g_str_has_suffix(shell_executable, "zsh"))
    {
        g_autofree gchar *config_dir = g_build_filename(g_get_user_config_dir(), "nemo", NULL);
        g_autofree gchar *zshrc_path = g_build_filename(config_dir, ".zshrc", NULL);
        g_autofree gchar *zshrc_content = NULL;
        g_autofree gchar *zdotdir_env = NULL;

        g_mkdir_with_parents(config_dir, 0700);

        zshrc_content = g_strdup_printf("_nemo_vte_update_cwd() { echo -en \"\\033]7;file://$PWD\\007\"; };\n"
                                      "typeset -a precmd_functions;\n"
                                      "if [[ -z \"$precmd_functions[(r)_nemo_vte_update_cwd]\" ]]; then\n"
                                      "  precmd_functions+=(_nemo_vte_update_cwd);\n"
                                      "fi;\n"
                                      "[ -f \"$HOME/.zshrc\" ] && . \"$HOME/.zshrc\";\n");

        g_file_set_contents(zshrc_path, zshrc_content, -1, NULL);

        zdotdir_env = g_strdup_printf("ZDOTDIR=%s", config_dir);
        gchar *zsh_envp[] = { "TERM=xterm-256color", "COLORTERM=truecolor", zdotdir_env, NULL };
        envp = g_strdupv(zsh_envp);
        argv = g_strsplit(shell_executable, " ", -1);
    }
    else /* Assume bash or other compatible shells */
    {
        argv = g_strsplit(shell_executable, " ", -1);
        gchar *bash_envp[] = {
            "TERM=xterm-256color",
            "COLORTERM=truecolor",
            "PROMPT_COMMAND=echo -en \"\\033]7;file://$PWD\\007\"",
            NULL
        };
        envp = g_strdupv(bash_envp);
    }

    vte_terminal_spawn_async(priv->terminal,
                             VTE_PTY_DEFAULT,
                             working_directory,
                             argv,
                             envp,
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL, NULL,
                             -1,
                             priv->spawn_cancellable,
                             (VteTerminalSpawnAsyncCallback)spawn_async_callback,
                             self);

    g_strfreev(argv);
    g_strfreev(envp);
}

static void
_clear_ssh_connection_data(NemoTerminalWidgetPrivate *priv)
{
    g_clear_pointer(&priv->ssh_hostname, g_free);
    g_clear_pointer(&priv->ssh_username, g_free);
    g_clear_pointer(&priv->ssh_port, g_free);
    g_clear_pointer(&priv->ssh_remote_path, g_free);
    g_clear_pointer(&priv->pending_ssh_hostname, g_free);
    g_clear_pointer(&priv->pending_ssh_username, g_free);
    g_clear_pointer(&priv->pending_ssh_port, g_free);
}

static void
_reset_to_local_state(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;

    _clear_ssh_connection_data(priv);
    priv->ssh_sync_mode = NEMO_TERMINAL_SYNC_NONE;

    priv->state = NEMO_TERMINAL_STATE_LOCAL;
    gtk_widget_hide(priv->ssh_indicator);
    priv->needs_respawn = TRUE;
}

static void
on_terminal_child_exited(VteTerminal *terminal, gint status, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;

    if (gtk_widget_in_destruction(GTK_WIDGET(self)))
    {
        priv->child_pid = -1;
        return;
    }

    priv->child_pid = -1;

    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        _reset_to_local_state(self);
        if (priv->is_visible)
        {
            spawn_terminal_async(self);
        }
    }
    else if (priv->state == NEMO_TERMINAL_STATE_LOCAL)
    {
        priv->needs_respawn = TRUE;
        if (priv->is_visible)
        {
            nemo_terminal_widget_toggle_visible(self);
        }
    }
}

static void
on_terminal_preference_changed(GSettings *settings, const gchar *key, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;

    if (g_strcmp0(key, "local-terminal-sync-mode") == 0)
    {
        priv->local_sync_mode = g_settings_get_enum(settings, "local-terminal-sync-mode");
    }
    else if (g_strcmp0(key, "ssh-terminal-auto-connect-mode") == 0)
    {
        priv->ssh_auto_connect_mode = g_settings_get_enum(settings, "ssh-terminal-auto-connect-mode");
    }
    else if (g_strcmp0(key, "terminal-font") == 0 || g_strcmp0(key, "terminal-font-size") == 0)
    {
        setup_terminal_font(priv->terminal);
    }
    else if (g_strcmp0(key, "terminal-color-scheme") == 0)
    {
        g_free(priv->color_scheme);
        priv->color_scheme = g_settings_get_string(settings, "terminal-color-scheme");
        nemo_terminal_widget_apply_color_scheme(self);
    }
}

static void
_sync_terminal_to_fm (NemoTerminalWidget *self, const gchar *cwd_uri)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_autoptr(GFile) new_gfile_location = NULL;
    gboolean should_sync_to_fm = FALSE;

    if (!cwd_uri)
    {
        return;
    }

    if (priv->ignore_next_terminal_cd_signal)
    {
        priv->ignore_next_terminal_cd_signal = FALSE;
        return;
    }

    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        if (priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_TERM_TO_FM)
        {
            should_sync_to_fm = TRUE;
            if (g_str_has_prefix(cwd_uri, "file://"))
            {
                g_autofree gchar *local_path = g_filename_from_uri(cwd_uri, NULL, NULL);
                if (local_path && priv->ssh_hostname)
                {
                    g_autoptr(GString) sftp_uri = g_string_new("sftp://");
                    if (priv->ssh_username && *priv->ssh_username)
                        g_string_append_printf(sftp_uri, "%s@", priv->ssh_username);
                    g_string_append(sftp_uri, priv->ssh_hostname);
                    if (priv->ssh_port && *priv->ssh_port)
                        g_string_append_printf(sftp_uri, ":%s", priv->ssh_port);
                    g_string_append(sftp_uri, local_path);
                    new_gfile_location = g_file_new_for_uri(sftp_uri->str);
                }
            }
        }
    }
    else
    {
        if (priv->local_sync_mode == NEMO_TERMINAL_SYNC_BOTH || priv->local_sync_mode == NEMO_TERMINAL_SYNC_TERM_TO_FM)
        {
            should_sync_to_fm = TRUE;
            new_gfile_location = g_file_new_for_uri(cwd_uri);
        }
    }

    if (should_sync_to_fm && new_gfile_location && (priv->current_location == NULL || !g_file_equal(new_gfile_location, priv->current_location)))
    {
        g_set_object(&priv->current_location, new_gfile_location);
        g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_CURRENT_LOCATION]);
        g_signal_emit(self, signals[CHANGE_DIRECTORY], 0, new_gfile_location);
    }
}

static void
on_terminal_directory_changed(VteTerminal *terminal, gpointer user_data)
{
    const gchar *cwd_uri_str = vte_terminal_get_current_directory_uri(terminal);
    _sync_terminal_to_fm(NEMO_TERMINAL_WIDGET(user_data), cwd_uri_str);
}

static void
feed_cd_command(VteTerminal *terminal, const char *path)
{
    g_return_if_fail(VTE_IS_TERMINAL(terminal));
    g_return_if_fail(path != NULL);

    g_autofree gchar *quoted_path = g_shell_quote(path);
    g_autofree gchar *cd_command_str = g_strdup_printf(" cd %s\r", quoted_path);

    vte_terminal_feed_child(terminal, SHELL_CTRL_A, -1);
    vte_terminal_feed_child(terminal, " ", -1);
    vte_terminal_feed_child(terminal, SHELL_CTRL_A, -1);
    vte_terminal_feed_child(terminal, SHELL_CTRL_K, -1);
    vte_terminal_feed_child(terminal, cd_command_str, -1);
    vte_terminal_feed_child(terminal, SHELL_CTRL_Y, -1);
    vte_terminal_feed_child(terminal, SHELL_CTRL_A, -1);
    vte_terminal_feed_child(terminal, SHELL_DELETE, -1);
    vte_terminal_feed_child(terminal, SHELL_CTRL_E, -1);
}

static gboolean
on_terminal_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;

    if ((event->state & GDK_CONTROL_MASK) && (event->state & GDK_SHIFT_MASK))
    {
        switch (event->keyval)
        {
            case GDK_KEY_C:
            case GDK_KEY_c:
                vte_terminal_copy_clipboard_format(priv->terminal, VTE_FORMAT_TEXT);
                return TRUE;
            case GDK_KEY_V:
            case GDK_KEY_v:
                vte_terminal_paste_clipboard(priv->terminal);
                return TRUE;
        }
    }
    return FALSE;
}

static void
on_ssh_exit_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;
    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        vte_terminal_feed_child(priv->terminal, " exit\n", -1);
    }
}

static GtkWidget *
_create_radio_menu_item(GSList **group, const gchar *label, gboolean is_active, GCallback activate_callback, gpointer user_data, const gchar *data_key, gpointer data_value)
{
    GtkWidget *item = gtk_radio_menu_item_new_with_label(*group, label);
    if (*group == NULL)
        *group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));

    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), is_active);
    g_signal_connect(item, "activate", activate_callback, user_data);
    g_object_set_data(G_OBJECT(item), data_key, data_value);

    return item;
}

static void on_ssh_connect_activate(GtkMenuItem *menuitem, gpointer user_data);

static GtkWidget *
_build_color_scheme_submenu(NemoTerminalWidget *self)
{
    GtkWidget *submenu = gtk_menu_new();
    GSList *radio_group = NULL;
    const gchar *current_scheme = nemo_terminal_widget_get_color_scheme(self);

    for (gsize i = 0; i < G_N_ELEMENTS(COLOR_SCHEME_ENTRIES); ++i)
    {
        GtkWidget *item = _create_radio_menu_item(&radio_group,
                                                  _(COLOR_SCHEME_ENTRIES[i].label_pot),
                                                  g_strcmp0(current_scheme, COLOR_SCHEME_ENTRIES[i].id) == 0,
                                                  G_CALLBACK(on_color_scheme_changed),
                                                  self,
                                                  DATA_KEY_VALUE,
                                                  (gpointer)COLOR_SCHEME_ENTRIES[i].id);
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    }
    return submenu;
}

static int
get_terminal_font_size(void)
{
    int saved_size_pts = g_settings_get_int(nemo_window_state, "terminal-font-size");
    return CLAMP(saved_size_pts, MIN_FONT_SIZE, MAX_FONT_SIZE);
}

static GtkWidget *
_build_font_size_submenu(NemoTerminalWidget *self)
{
    GtkWidget *submenu = gtk_menu_new();
    GSList *radio_group = NULL;
    int current_size_pts = get_terminal_font_size();

    for (gsize i = 0; i < G_N_ELEMENTS(FONT_SIZE_ENTRIES); ++i)
    {
        g_autofree gchar *label = g_strdup_printf("%d", FONT_SIZE_ENTRIES[i].size_pts);
        GtkWidget *item = _create_radio_menu_item(&radio_group,
                                                  label,
                                                  current_size_pts == FONT_SIZE_ENTRIES[i].size_pts,
                                                  G_CALLBACK(on_font_size_changed),
                                                  self,
                                                  DATA_KEY_VALUE,
                                                  GINT_TO_POINTER(FONT_SIZE_ENTRIES[i].size_pts));
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    }
    return submenu;
}

static GtkWidget *
_build_enum_pref_submenu(NemoTerminalWidget *self, const MenuSyncModeEntry *entries, gsize count, gint current_value, const gchar *settings_key)
{
    GtkWidget *submenu = gtk_menu_new();
    GSList *radio_group = NULL;
    for (gsize i = 0; i < count; ++i)
    {
        GtkWidget *item = _create_radio_menu_item(&radio_group,
                                                  _(entries[i].label_pot),
                                                  current_value == entries[i].mode,
                                                  G_CALLBACK(on_enum_pref_changed),
                                                  (gpointer)settings_key,
                                                  DATA_KEY_VALUE,
                                                  GINT_TO_POINTER(entries[i].mode));
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    }
    return submenu;
}

static GtkWidget *
_build_local_sync_submenu(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    return _build_enum_pref_submenu(self, LOCAL_SYNC_MODE_ENTRIES, G_N_ELEMENTS(LOCAL_SYNC_MODE_ENTRIES), priv->local_sync_mode, "local-terminal-sync-mode");
}

static GtkWidget *
_build_sftp_auto_connect_submenu(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    return _build_enum_pref_submenu(self, (const MenuSyncModeEntry*)SFTP_AUTO_CONNECT_ENTRIES, G_N_ELEMENTS(SFTP_AUTO_CONNECT_ENTRIES), priv->ssh_auto_connect_mode, "ssh-terminal-auto-connect-mode");
}

static GtkWidget *
_build_manual_ssh_connect_submenu(NemoTerminalWidget *self, const gchar *hostname, const gchar *username, const gchar *port)
{
    GtkWidget *submenu = gtk_menu_new();
    for (gsize i = 0; i < G_N_ELEMENTS(MANUAL_SSH_SYNC_ENTRIES); ++i)
    {
        GtkWidget *item = gtk_menu_item_new_with_label(_(MANUAL_SSH_SYNC_ENTRIES[i].label_pot));
        g_object_set_data_full(G_OBJECT(item), DATA_KEY_SSH_HOSTNAME, g_strdup(hostname), g_free);
        g_object_set_data_full(G_OBJECT(item), DATA_KEY_SSH_USERNAME, g_strdup(username), g_free);
        g_object_set_data_full(G_OBJECT(item), DATA_KEY_SSH_PORT, g_strdup(port), g_free);
        g_object_set_data(G_OBJECT(item), DATA_KEY_SSH_SYNC_MODE, GINT_TO_POINTER(MANUAL_SSH_SYNC_ENTRIES[i].mode));
        g_signal_connect(item, "activate", G_CALLBACK(on_ssh_connect_activate), self);
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
    }
    return submenu;
}

static void
on_copy_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_copy_clipboard_format(self->priv->terminal, VTE_FORMAT_TEXT);
}

static void
on_paste_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_paste_clipboard(self->priv->terminal);
}

static void
on_select_all_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_select_all(self->priv->terminal);
}

static void
_append_menu_item(GtkMenuShell *menu, const gchar *label, GCallback callback, gpointer user_data, gboolean sensitive)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", callback, user_data);
    gtk_widget_set_sensitive(item, sensitive);
    gtk_menu_shell_append(menu, item);
}

static void
_append_menu_item_with_submenu(GtkMenuShell *menu, const gchar *label, GtkWidget *submenu)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
    gtk_menu_shell_append(menu, item);
}

static GtkWidget *
create_terminal_popup_menu(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    GtkWidget *menu = gtk_menu_new();
    gboolean is_sftp_location = FALSE;

    _append_menu_item(GTK_MENU_SHELL(menu), _("Copy"), G_CALLBACK(on_copy_activate), self, vte_terminal_get_has_selection(priv->terminal));
    _append_menu_item(GTK_MENU_SHELL(menu), _("Paste"), G_CALLBACK(on_paste_activate), self, TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    _append_menu_item(GTK_MENU_SHELL(menu), _("Select All"), G_CALLBACK(on_select_all_activate), self, TRUE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    _append_menu_item_with_submenu(GTK_MENU_SHELL(menu), _("Color Scheme"), _build_color_scheme_submenu(self));
    _append_menu_item_with_submenu(GTK_MENU_SHELL(menu), _("Font Size"), _build_font_size_submenu(self));
    _append_menu_item_with_submenu(GTK_MENU_SHELL(menu), _("SSH Auto-Connect"), _build_sftp_auto_connect_submenu(self));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    if (priv->current_location)
    {
        g_autofree gchar *scheme = g_file_get_uri_scheme(priv->current_location);
        if (scheme && g_strcmp0(scheme, "sftp") == 0)
            is_sftp_location = TRUE;
    }

    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        _append_menu_item(GTK_MENU_SHELL(menu), _("Disconnect from SSH"), G_CALLBACK(on_ssh_exit_activate), self, TRUE);
    }
    else
    {
        _append_menu_item_with_submenu(GTK_MENU_SHELL(menu), _("Local Folder Sync"), _build_local_sync_submenu(self));
        if (is_sftp_location)
        {
            g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;
            if (parse_gvfs_ssh_path(priv->current_location, &hostname, &username, &port))
            {
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
                g_autofree gchar *label = g_strdup_printf(_("SSH Connection to %s"), hostname);
                _append_menu_item_with_submenu(GTK_MENU_SHELL(menu), label, _build_manual_ssh_connect_submenu(self, hostname, username, port));
            }
        }
    }

    gtk_widget_show_all(menu);
    return menu;
}

static gboolean
on_terminal_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    if (event->button == GDK_BUTTON_SECONDARY && event->type == GDK_BUTTON_PRESS)
    {
        GtkWidget *menu = create_terminal_popup_menu(self);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
        return TRUE;
    }
    return FALSE;
}

static gchar *
get_remote_path_from_sftp_gfile(GFile *location)
{
    g_autoptr(GUri) uri = NULL;
    g_autofree gchar *uri_str = g_file_get_uri(location);
    if (!uri_str)
        return NULL;

    uri = g_uri_parse(uri_str, G_URI_FLAGS_NONE, NULL);
    if (uri)
        return g_strdup(g_uri_get_path(uri));

    return NULL;
}

static void
change_directory_in_terminal (NemoTerminalWidget *self, GFile *location)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_autofree gchar *target_path = NULL;
    gboolean should_sync = FALSE;
    g_autoptr(GFile) dir_location = NULL;

    if (!priv->is_visible || priv->child_pid == -1 || !location)
        return;

    g_autoptr(GFileInfo) info = g_file_query_info(location, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
    if (info && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) {
        dir_location = g_object_ref(location);
    } else {
        dir_location = g_file_get_parent(location);
    }

    if (!dir_location) {
        return;
    }

    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        if (priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_FM_TO_TERM)
        {
            should_sync = TRUE;
            target_path = get_remote_path_from_sftp_gfile(dir_location);
        }
    }
    else
    {
        if (priv->local_sync_mode == NEMO_TERMINAL_SYNC_BOTH || priv->local_sync_mode == NEMO_TERMINAL_SYNC_FM_TO_TERM)
        {
            if (g_file_query_exists(dir_location, NULL))
            {
                target_path = g_file_get_path(dir_location);
                if (target_path != NULL)
                {
                    should_sync = TRUE;
                }
            }
        }
    }

    if (should_sync && target_path)
    {
        const gchar *term_uri_str = vte_terminal_get_current_directory_uri(priv->terminal);
        g_autoptr(GFile) term_gfile = term_uri_str ? g_file_new_for_uri(term_uri_str) : NULL;
        g_autofree gchar *term_path = term_gfile ? g_file_get_path(term_gfile) : NULL;

        if (term_path == NULL || g_strcmp0(term_path, target_path) != 0)
        {
            priv->ignore_next_terminal_cd_signal = TRUE;
            feed_cd_command(priv->terminal, target_path);
        }
    }
}

static gboolean
parse_gvfs_ssh_path(GFile *location, gchar **hostname, gchar **username, gchar **port)
{
    g_autoptr(GUri) uri = NULL;
    g_autofree gchar *uri_str = g_file_get_uri(location);

    *hostname = NULL;
    *username = NULL;
    *port = NULL;

    if (!uri_str)
        return FALSE;

    uri = g_uri_parse(uri_str, G_URI_FLAGS_NONE, NULL);
    if (uri && g_strcmp0(g_uri_get_scheme(uri), "sftp") == 0)
    {
        *hostname = g_strdup(g_uri_get_host(uri));
        if (g_uri_get_userinfo(uri))
            *username = g_strdup(g_uri_get_userinfo(uri));
        if (g_uri_get_port(uri) > 0)
            *port = g_strdup_printf("%d", g_uri_get_port(uri));
        return (*hostname != NULL);
    }

    return FALSE;
}

static void
_initiate_ssh_connection(NemoTerminalWidget *self, const gchar *hostname, const gchar *username, const gchar *port, NemoTerminalSyncMode sync_mode)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_autoptr(GPtrArray) argv_array = g_ptr_array_new_with_free_func(g_free);
    g_autofree gchar *remote_cmd = NULL;
    GString *remote_cmd_builder;

    if (priv->child_pid != -1)
    {
        kill(priv->child_pid, SIGTERM);
        priv->child_pid = -1;
    }

    priv->state = NEMO_TERMINAL_STATE_IN_SSH;
    priv->ssh_sync_mode = sync_mode;
    priv->ssh_hostname = g_strdup(hostname);
    priv->ssh_username = g_strdup(username);
    priv->ssh_port = g_strdup(port);

    if (priv->current_location)
        priv->ssh_remote_path = get_remote_path_from_sftp_gfile(priv->current_location);

    remote_cmd_builder = g_string_new("");
    if (priv->ssh_remote_path && *priv->ssh_remote_path)
    {
        g_autofree gchar *quoted_remote_path = g_shell_quote(priv->ssh_remote_path);
        g_string_append_printf(remote_cmd_builder, " cd %s; ", quoted_remote_path);
    }
    if (priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == NEMO_TERMINAL_SYNC_TERM_TO_FM)
    {
        g_string_append(remote_cmd_builder, " export PROMPT_COMMAND='echo -en \"\\033]7;file://$PWD\\007\"'; ");
    }
    g_string_append(remote_cmd_builder, "$SHELL -l");
    remote_cmd = g_string_free(remote_cmd_builder, FALSE);

    g_ptr_array_add(argv_array, g_strdup("ssh"));
    g_ptr_array_add(argv_array, g_strdup("-t"));
    if (port && *port)
    {
        g_ptr_array_add(argv_array, g_strdup("-p"));
        g_ptr_array_add(argv_array, g_strdup(port));
    }
    if (username && *username)
        g_ptr_array_add(argv_array, g_strdup_printf("%s@%s", username, hostname));
    else
        g_ptr_array_add(argv_array, g_strdup(hostname));

    g_ptr_array_add(argv_array, g_strdup(remote_cmd));
    g_ptr_array_add(argv_array, NULL);

    gtk_widget_show(priv->ssh_indicator);

    priv->ignore_next_terminal_cd_signal = TRUE;

    vte_terminal_spawn_async(priv->terminal,
                             VTE_PTY_DEFAULT,
                             NULL,
                             (gchar **)argv_array->pdata,
                             NULL,
                             G_SPAWN_SEARCH_PATH,
                             NULL, NULL, NULL,
                             -1,
                             priv->spawn_cancellable,
                             (VteTerminalSpawnAsyncCallback)spawn_async_callback,
                             self);
    nemo_terminal_widget_ensure_terminal_focus(self);
}

static void
save_terminal_font_size(int font_size_pts)
{
    g_settings_set_int(nemo_window_state, "terminal-font-size", font_size_pts);
}

static void
on_color_scheme_changed(GtkCheckMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    if (gtk_check_menu_item_get_active(menuitem))
    {
        const gchar *scheme_name = g_object_get_data(G_OBJECT(menuitem), DATA_KEY_VALUE);
        nemo_terminal_widget_set_color_scheme(self, scheme_name);
    }
}

static void
on_font_size_changed(GtkCheckMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    NemoTerminalWidgetPrivate *priv = self->priv;
    if (gtk_check_menu_item_get_active(menuitem))
    {
        int font_size_pts = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), DATA_KEY_VALUE));
        g_autoptr(PangoFontDescription) font_desc = pango_font_description_copy(vte_terminal_get_font(priv->terminal));
        pango_font_description_set_size(font_desc, font_size_pts * PANGO_SCALE);
        vte_terminal_set_font(priv->terminal, font_desc);
        save_terminal_font_size(font_size_pts);
    }
}

static void
on_enum_pref_changed(GtkCheckMenuItem *menuitem, gpointer user_data)
{
    const gchar *pref_name = user_data;
    if (gtk_check_menu_item_get_active(menuitem))
    {
        gint new_value = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), DATA_KEY_VALUE));
        g_settings_set_enum(nemo_window_state, pref_name, new_value);
    }
}

static void
on_ssh_connect_activate(GtkMenuItem *menuitem, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    const gchar *hostname = g_object_get_data(G_OBJECT(menuitem), DATA_KEY_SSH_HOSTNAME);
    const gchar *username = g_object_get_data(G_OBJECT(menuitem), DATA_KEY_SSH_USERNAME);
    const gchar *port = g_object_get_data(G_OBJECT(menuitem), DATA_KEY_SSH_PORT);
    NemoTerminalSyncMode sync_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), DATA_KEY_SSH_SYNC_MODE));

    if (hostname)
        _initiate_ssh_connection(self, hostname, username, port, sync_mode);
}

static void
setup_terminal_font(VteTerminal *terminal)
{
    g_autoptr(PangoFontDescription) font_desc = NULL;
    g_autofree gchar *font_name = g_settings_get_string(nemo_window_state, "terminal-font");
    int font_size = get_terminal_font_size();

    if (font_name && *font_name) {
        font_desc = pango_font_description_from_string(font_name);
    } else {
        font_desc = pango_font_description_from_string("Monospace");
    }

    pango_font_description_set_size(font_desc, font_size * PANGO_SCALE);
    vte_terminal_set_font(terminal, font_desc);
}

static const gchar *
nemo_terminal_widget_get_color_scheme(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    if (priv->color_scheme == NULL) {
        priv->color_scheme = g_settings_get_string(nemo_window_state, "terminal-color-scheme");
    }
    return priv->color_scheme;
}

static void
nemo_terminal_widget_set_color_scheme(NemoTerminalWidget *self, const gchar *scheme)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    g_settings_set_string(nemo_window_state, "terminal-color-scheme", scheme);
    g_free(priv->color_scheme);
    priv->color_scheme = g_strdup(scheme);
    nemo_terminal_widget_apply_color_scheme(self);
}

static void
nemo_terminal_widget_apply_color_scheme(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv = self->priv;
    const gchar *scheme_id = nemo_terminal_widget_get_color_scheme(self);
    const MenuSchemeEntry *scheme = NULL;
    GtkStyleContext *context;
    GdkRGBA fg, bg;

    for (gsize i = 0; i < G_N_ELEMENTS(COLOR_SCHEME_ENTRIES); ++i) {
        if (g_strcmp0(COLOR_SCHEME_ENTRIES[i].id, scheme_id) == 0) {
            scheme = &COLOR_SCHEME_ENTRIES[i];
            break;
        }
    }

    if (scheme == NULL) { /* Fallback to system */
        scheme = &COLOR_SCHEME_ENTRIES[0];
    }

    if (scheme->palette.use_system_colors) {
        context = gtk_widget_get_style_context(GTK_WIDGET(priv->terminal));
        gtk_style_context_get_color(context, gtk_widget_get_state_flags(GTK_WIDGET(priv->terminal)), &fg);
        gtk_style_context_get_background_color(context, gtk_widget_get_state_flags(GTK_WIDGET(priv->terminal)), &bg);
        vte_terminal_set_colors(priv->terminal, &fg, &bg, NULL, 0);
    } else {
        vte_terminal_set_colors(priv->terminal,
                                &scheme->palette.foreground,
                                &scheme->palette.background,
                                (GdkRGBA *)scheme->palette.palette,
                                G_N_ELEMENTS(scheme->palette.palette));
    }
}

NemoTerminalWidget *
nemo_terminal_widget_new(void)
{
    return g_object_new(NEMO_TYPE_TERMINAL_WIDGET, NULL);
}

NemoTerminalWidget *
nemo_terminal_widget_new_with_location(GFile *location)
{
    return g_object_new(NEMO_TYPE_TERMINAL_WIDGET, "current-location", location, NULL);
}

void
nemo_terminal_widget_set_current_location(NemoTerminalWidget *self, GFile *location)
{
    NemoTerminalWidgetPrivate *priv;

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;

    if ((priv->current_location == location) || (priv->current_location && location && g_file_equal(priv->current_location, location)))
    {
        return;
    }

    g_set_object(&priv->current_location, location);
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_CURRENT_LOCATION]);

    if (!location)
    {
        return;
    }

    g_autofree gchar *scheme = g_file_get_uri_scheme(location);
    gboolean is_sftp = (scheme && g_strcmp0(scheme, "sftp") == 0);

    if (priv->state == NEMO_TERMINAL_STATE_IN_SSH)
    {
        if (is_sftp)
        {
            g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;
            if (parse_gvfs_ssh_path(location, &hostname, &username, &port) &&
                priv->ssh_hostname && g_strcmp0(hostname, priv->ssh_hostname) == 0)
            {
                change_directory_in_terminal(self, location);
            }
        }
        /* If not SFTP or different host, do nothing. Keep the current SSH session. */
    }
    else /* Local state */
    {
        if (is_sftp && priv->ssh_auto_connect_mode != NEMO_TERMINAL_SSH_AUTOCONNECT_OFF)
        {
            g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;
            if (parse_gvfs_ssh_path(location, &hostname, &username, &port))
            {
                NemoTerminalSyncMode sync_mode;
                switch (priv->ssh_auto_connect_mode) {
                    case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH: sync_mode = NEMO_TERMINAL_SYNC_BOTH; break;
                    case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM: sync_mode = NEMO_TERMINAL_SYNC_FM_TO_TERM; break;
                    case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM: sync_mode = NEMO_TERMINAL_SYNC_TERM_TO_FM; break;
                    default: sync_mode = NEMO_TERMINAL_SYNC_NONE; break;
                }

                if (priv->child_pid != -1) {
                    _initiate_ssh_connection(self, hostname, username, port, sync_mode);
                } else {
                    priv->pending_ssh_hostname = g_strdup(hostname);
                    priv->pending_ssh_username = g_strdup(username);
                    priv->pending_ssh_port = g_strdup(port);
                    priv->pending_ssh_sync_mode = sync_mode;
                }
            }
        }
        else
        {
            change_directory_in_terminal(self, location);
        }
    }
}

void
nemo_terminal_widget_set_container_paned(NemoTerminalWidget *self, GtkWidget *paned)
{
    NemoTerminalWidgetPrivate *priv;
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;
    g_weak_ref_set(&priv->paned_weak_ref, paned);
}

void
nemo_terminal_widget_toggle_visible(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv;
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;

    if (priv->in_toggling)
        return;
    priv->in_toggling = TRUE;

    priv->is_visible = !priv->is_visible;

    if (priv->is_visible)
    {
        gtk_widget_show(GTK_WIDGET(self));
        if (priv->needs_respawn)
        {
            spawn_terminal_async(self);
        }
        else if (priv->current_location)
        {
            change_directory_in_terminal(self, priv->current_location);
        }
        nemo_terminal_widget_apply_new_size(self);
        nemo_terminal_widget_ensure_terminal_focus(self);
    }
    else
    {
        g_autoptr(GtkWidget) paned = g_weak_ref_get(&priv->paned_weak_ref);
        if (paned && GTK_IS_PANED(paned)) {
            int position = gtk_paned_get_position(GTK_PANED(paned));
            int total_height = gtk_widget_get_allocated_height(paned);
            if (total_height > 0) {
                g_settings_set_int(nemo_window_state, "terminal-pane-size", total_height - position);
            }
        }
        gtk_widget_hide(GTK_WIDGET(self));
    }

    g_settings_set_boolean(nemo_window_state, "terminal-visible", priv->is_visible);
    g_signal_emit(self, signals[TOGGLE_VISIBILITY], 0, priv->is_visible);
    priv->in_toggling = FALSE;
}

void
nemo_terminal_widget_ensure_state(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv;
    gboolean should_be_visible;

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;

    should_be_visible = g_settings_get_boolean(nemo_window_state, "terminal-visible");

    if (priv->is_visible != should_be_visible)
    {
        priv->is_visible = should_be_visible;
        g_signal_emit(self, signals[TOGGLE_VISIBILITY], 0, priv->is_visible);
    }

    if (should_be_visible) {
        gtk_widget_show(GTK_WIDGET(self));
        if (priv->needs_respawn) {
            spawn_terminal_async(self);
        }
    } else {
        gtk_widget_hide(GTK_WIDGET(self));
    }
}

static gboolean
_ensure_focus_timeout(gpointer user_data)
{
    NemoTerminalWidget *self = user_data;
    NemoTerminalWidgetPrivate *priv = self->priv;

    if (priv && gtk_widget_get_window(GTK_WIDGET(priv->terminal)))
    {
        gtk_widget_grab_focus(GTK_WIDGET(priv->terminal));
        priv->focus_timeout_id = 0;
    }
    return G_SOURCE_REMOVE;
}

void
nemo_terminal_widget_ensure_terminal_focus(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv;
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;

    if (priv->focus_timeout_id > 0) {
        g_source_remove(priv->focus_timeout_id);
    }
    priv->focus_timeout_id = g_timeout_add(100, _ensure_focus_timeout, self);
}

gboolean
nemo_terminal_widget_get_visible(NemoTerminalWidget *self)
{
    g_return_val_if_fail(NEMO_IS_TERMINAL_WIDGET(self), FALSE);
    return self->priv->is_visible;
}

void
nemo_terminal_widget_apply_new_size(NemoTerminalWidget *self)
{
    NemoTerminalWidgetPrivate *priv;
    g_autoptr(GtkWidget) paned = NULL;
    gint term_size;

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    priv = self->priv;

    paned = g_weak_ref_get(&priv->paned_weak_ref);
    if (!paned || !GTK_IS_PANED(paned) || !gtk_widget_get_realized(paned)) {
        return;
    }

    if (priv->is_visible) {
        gint total_height = gtk_widget_get_allocated_height(paned);
        term_size = g_settings_get_int(nemo_window_state, "terminal-pane-size");
        if (term_size <= MIN_TERMINAL_HEIGHT) {
            term_size = 200; /* Default fallback */
        }

        gint max_allowed_height = MAX(total_height - MIN_MAIN_VIEW_HEIGHT, MIN_TERMINAL_HEIGHT);
        gint terminal_height = CLAMP(term_size, MIN_TERMINAL_HEIGHT, max_allowed_height);

        gtk_paned_set_position(GTK_PANED(paned), total_height - terminal_height);
    }
}
