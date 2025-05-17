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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <vte/vte.h>
#include <libnemo-private/nemo-global-preferences.h>
#include "nemo-window.h"
#include "nemo-window-slot.h"

/* Data keys for g_object_set_data / g_object_get_data */
static const gchar * const DATA_KEY_SSH_HOSTNAME = "ssh-hostname";
static const gchar * const DATA_KEY_SSH_USERNAME = "ssh-username";
static const gchar * const DATA_KEY_SSH_PORT = "ssh-port";
static const gchar * const DATA_KEY_SSH_SYNC_MODE = "ssh-sync-mode";
static const gchar * const DATA_KEY_SCHEME_NAME = "scheme-name";
static const gchar * const DATA_KEY_FONT_SIZE = "font-size";
static const gchar * const DATA_KEY_LOCAL_SYNC_MODE = "local-sync-mode";
static const gchar * const DATA_KEY_SFTP_AUTO_CONNECT_MODE = "sftp-auto-connect-mode";

/* Forward declarations */

/* Action Callbacks */
static void on_copy_activate           (GSimpleAction  *action,
                                        GVariant       *parameter,
                                        gpointer        user_data);
static void on_paste_activate          (GSimpleAction  *action,
                                        GVariant       *parameter,
                                        gpointer        user_data);
static void on_select_all_activate     (GSimpleAction  *action,
                                        GVariant       *parameter,
                                        gpointer        user_data);

/* Terminal Event Callbacks */
static gboolean on_terminal_button_press(GtkWidget *widget,
                                         GdkEventButton *event,
                                         gpointer user_data);
static gboolean on_terminal_key_press(GtkWidget *widget,
                                      GdkEventKey *event,
                                      gpointer user_data);
static void on_terminal_contents_changed(VteTerminal *terminal,
                                         gpointer user_data);
static void on_terminal_directory_changed(VteTerminal *terminal,
                                          gpointer user_data);
static void on_terminal_child_exited(VteTerminal *terminal,
                                     gint status,
                                     gpointer user_data);

/* Menu Item Callbacks */
static void on_color_scheme_changed(GtkWidget *widget,
                                    gpointer user_data);
static void on_font_size_changed(GtkWidget *widget,
                                 gpointer user_data);
static void on_local_sync_mode_changed(GtkWidget *widget,
                                       gpointer user_data);
static void on_sftp_auto_connect_behavior_changed(GtkWidget *widget,
                                                  gpointer user_data);
static void on_ssh_connect_activate(GtkWidget *widget, /* GtkMenuItem */
                                    gpointer user_data);
static void on_ssh_exit_activate(GtkWidget *widget, /* GtkMenuItem */
                                 gpointer user_data);
static void _on_menu_item_activate_widget_action (GtkMenuItem *menuitem,
                                                  gpointer     user_data);

/* SSH Helper Functions */
static void _initiate_ssh_connection(NemoTerminalWidget *self,
                                     const gchar *hostname,
                                     const gchar *username,
                                     const gchar *port,
                                     NemoTerminalSyncMode sync_mode);
static void clear_ssh_state(NemoTerminalWidget *self);
static gchar *build_ssh_command_string(const gchar *hostname,
                                       const gchar *username,
                                       const gchar *port);
static gboolean parse_gvfs_ssh_path(GFile *location,
                                    gchar **hostname,
                                    gchar **username,
                                    gchar **port);
static gchar *get_remote_path_from_sftp_gfile(GFile *location);

/* Directory and Command Helper Functions */
static void change_directory_in_terminal(NemoTerminalWidget *self, GFile *location);
static void feed_cd_command(VteTerminal *terminal, const char *path);

/* UI and State Helper Functions */
static void setup_terminal_font(VteTerminal *terminal);
static int nemo_terminal_get_font_size(void);
static void nemo_terminal_widget_save_font_size(NemoTerminalWidget *self, int font_size);
static void reset_terminal_to_current_location(NemoTerminalWidget *self);
static gboolean focus_once_and_remove(gpointer widget_data);
static gboolean reset_toggling_flag(gpointer user_data);
static GtkWidget * create_terminal_popup_menu(NemoTerminalWidget *self);
static void on_container_size_changed(GtkPaned *paned, GParamSpec *pspec, gpointer user_data);
static void on_paned_destroy(GtkWidget *widget, gpointer user_data);
static gboolean apply_initial_size_idle(gpointer user_data);
static void _add_action_menu_item_compat(GtkMenuShell *menu_shell,
                                         NemoTerminalWidget *self,
                                         const gchar *label,
                                         const gchar *detailed_action_name);
static void _add_callback_menu_item(GtkMenuShell *menu_shell,
                                    const gchar *label,
                                    GCallback callback,
                                    gpointer user_data);
static GtkWidget *_add_radio_menu_item_with_data(GtkMenuShell *menu_shell,
                                                 GSList **radio_group,
                                                 const gchar *label,
                                                 const gchar *data_key,
                                                 gpointer data_value,
                                                 gboolean is_active,
                                                 GCallback activate_callback,
                                                 gpointer user_data);

/* GObject Lifecycle */
static void nemo_terminal_widget_finalize(GObject *object);


/**
 * on_ssh_connect_activate:
 * @widget: The GtkMenuItem that was activated.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for when an SSH connection menu item is activated.
 * Retrieves SSH connection details from the menu item's data and
 * initiates the SSH connection.
 */
static void
on_ssh_connect_activate(GtkWidget *widget,
                        gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    // Retrieve connection parameters stored on the menu item
    const gchar *hostname = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_HOSTNAME);
    const gchar *username = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_USERNAME);
    const gchar *port = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_PORT);
    NemoTerminalSyncMode sync_mode = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_SYNC_MODE));

    if (hostname != NULL)
    {
        _initiate_ssh_connection(self, hostname, username, port, sync_mode);
    }
    else
    {
        g_warning("Hostname not found on SSH menu item for manual connection.");
    }
}

/**
 * _initiate_ssh_connection:
 * @self: The #NemoTerminalWidget instance.
 * @hostname: The hostname to connect to.
 * @username: (Optional) The username for the SSH connection.
 * @port: (Optional) The port for the SSH connection.
 * @sync_mode: The directory synchronization mode to use for this SSH session.
 *
 * Builds and executes the SSH command in the terminal. Sets up SSH state
 * variables and prepares for directory synchronization if configured.
 * The command fed to the terminal includes "; exit" to ensure the shell
 * process hosting the ssh client exits, triggering on_terminal_child_exited.
 */
static void
_initiate_ssh_connection(NemoTerminalWidget *self,
                         const gchar *hostname,
                         const gchar *username,
                         const gchar *port,
                         NemoTerminalSyncMode sync_mode)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    g_return_if_fail(hostname != NULL && *hostname != '\0');

    // Build the basic "ssh user@host -p port\n" command
    gchar *ssh_command_line_nl = build_ssh_command_string(hostname, username, port);
    if (!ssh_command_line_nl)
    {
        g_warning("Failed to build SSH command string.");
        return;
    }

    // Append "; exit" to the SSH command.
    // This ensures that when the ssh client process finishes, the local shell
    // running it also exits, which correctly triggers 'on_terminal_child_exited'.
    g_autofree gchar *final_command_to_feed = NULL;
    if (g_str_has_suffix(ssh_command_line_nl, "\n"))
    {
        // Remove trailing '\n' from ssh_command_line_nl and append "; exit\n"
        GString *str_builder = g_string_new_len(ssh_command_line_nl, strlen(ssh_command_line_nl) - 1);
        g_string_append(str_builder, "; exit\n");
        final_command_to_feed = g_string_free(str_builder, FALSE);
    }
    else
    {
        // Fallback: Should not happen if build_ssh_command_string is consistent
        g_warning("_initiate_ssh_connection: ssh_command_line_nl did not end with newline as expected.");
        final_command_to_feed = g_strconcat(ssh_command_line_nl, "; exit\n", NULL);
    }
    g_free(ssh_command_line_nl); // Free the original command string

    if (!final_command_to_feed) {
        g_warning("Failed to build final SSH command string with ; exit.");
        return;
    }

    // Mark that we're in the process of connecting via SSH.
    // Full setup (like cd to remote path) happens after connection is established (see on_terminal_contents_changed).
    self->ssh_connecting = TRUE;
    self->pending_ssh_sync_mode = sync_mode;

    // Set SSH mode and store connection details
    self->in_ssh_mode = TRUE;
    self->ssh_sync_mode = sync_mode; // Set the determined sync mode for this session
    g_free(self->ssh_hostname);
    self->ssh_hostname = g_strdup(hostname);
    g_free(self->ssh_username);
    self->ssh_username = g_strdup(username);
    g_free(self->ssh_port);
    self->ssh_port = g_strdup(port);

    // If current location is SFTP, try to get remote path for potential `cd` after connection
    if (self->current_location && G_IS_FILE(self->current_location))
    {
        g_free(self->ssh_remote_path);
        self->ssh_remote_path = get_remote_path_from_sftp_gfile(self->current_location);
    }

    // Show SSH indicator in the UI
    if (self->ssh_indicator)
    {
        gtk_widget_show(self->ssh_indicator);
    }

    // Feed the complete command (e.g., "ssh user@host; exit\n") to the terminal
    vte_terminal_feed_child(self->terminal, final_command_to_feed, -1);
}

/**
 * clear_ssh_state:
 * @self: The #NemoTerminalWidget instance.
 *
 * Clears all SSH-related state variables in the widget,
 * effectively ending the SSH mode.
 */
static void
clear_ssh_state(NemoTerminalWidget *self)
{
    if (self->in_ssh_mode || self->ssh_connecting)
    {
        self->in_ssh_mode = FALSE;
        self->ssh_sync_mode = NEMO_TERMINAL_SYNC_NONE; // Reset to default
        g_clear_pointer(&self->ssh_hostname, g_free);
        g_clear_pointer(&self->ssh_username, g_free);
        g_clear_pointer(&self->ssh_port, g_free);
        g_clear_pointer(&self->ssh_remote_path, g_free);
        self->ssh_connecting = FALSE; // Ensure this is reset
        self->pending_ssh_sync_mode = NEMO_TERMINAL_SYNC_NONE;
    }
}

/**
 * reset_terminal_to_current_location:
 * @self: The #NemoTerminalWidget instance.
 *
 * Resets the terminal state, typically after an SSH session ends.
 * It clears SSH state, hides the SSH indicator, updates the terminal's
 * current location to match the file manager's active native location,
 * and spawns a new local shell.
 */
static void
reset_terminal_to_current_location(NemoTerminalWidget *self)
{
    NemoWindowSlot *slot = NULL;
    NemoWindow *win = NULL;

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    // Ignore the next cd signal that might be emitted by the shell startup
    self->ignore_next_terminal_cd_signal = TRUE;

    // Hide SSH indicator
    if (self->ssh_indicator) {
        gtk_widget_hide(self->ssh_indicator);
    }

    // Attempt to get the active Nemo window and slot to find the current FM location
    if (self->container_paned) {
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(self->container_paned));
        if (toplevel && NEMO_IS_WINDOW(toplevel)) {
            win = NEMO_WINDOW(toplevel);
            slot = nemo_window_get_active_slot(win);
        }
    }

    // Update current_location to the file manager's active native path
    if (slot && slot->location && G_IS_FILE(slot->location) && g_file_is_native(slot->location)) {
         g_set_object(&self->current_location, slot->location);
    } else {
         // Fallback to no specific location (will use home or default)
         g_set_object(&self->current_location, NULL);
    }

    clear_ssh_state(self); // Crucial: resets SSH mode flags and data
    spawn_terminal_in_widget(self); // Spawn a new local shell
}

/**
 * on_ssh_exit_activate:
 * @widget: The GtkMenuItem that was activated ("Disconnect from SSH").
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Handles the user's request to disconnect from an active SSH session.
 * Feeds an "exit" command to the terminal (intended for the remote shell)
 * and then resets the terminal to a local state.
 */
static void
on_ssh_exit_activate(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    if (!self->in_ssh_mode) return; // Not in SSH mode, nothing to exit

    self->is_exiting_ssh = TRUE; // Flag to manage state in on_terminal_child_exited
    self->ignore_next_terminal_cd_signal = TRUE; // Ignore cd from shell startup

    // Send "exit\n" to the terminal. This should terminate the remote shell.
    // The "; exit" part of the original ssh command will then cause the local
    // child process (that ran ssh) to exit, triggering on_terminal_child_exited.
    vte_terminal_feed_child(self->terminal, "exit\n", -1);

    // Proactively reset the terminal state. on_terminal_child_exited will see
    // is_exiting_ssh = TRUE and will not attempt another reset.
    reset_terminal_to_current_location(self);

    self->is_exiting_ssh = FALSE; // Reset the flag
}

/* Action entries for the terminal (copy, paste, select-all) */
static GActionEntry terminal_entries[] = {
    {"copy", on_copy_activate, NULL, NULL, NULL},
    {"paste", on_paste_activate, NULL, NULL, NULL},
    {"select-all", on_select_all_activate, NULL, NULL, NULL},
};

/* GObject properties */
enum
{
    PROP_0,
    PROP_CURRENT_LOCATION,
    N_PROPS
};

static GParamSpec *properties[N_PROPS];

/* GObject signals */
enum
{
    CHANGE_DIRECTORY,
    TOGGLE_VISIBILITY,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE(NemoTerminalWidget, nemo_terminal_widget, GTK_TYPE_BOX)

/*** Helper structs for menu creation ***/
typedef struct {
    const gchar *id;         // Internal identifier for the scheme
    const gchar *label_pot;  // Translatable label (e.g., N_("System"))
} MenuSchemeEntry;

static const MenuSchemeEntry COLOR_SCHEME_ENTRIES[] = {
    {"system", N_("System")},
    {"dark", N_("Dark")},
    {"light", N_("Light")},
    {"solarized-dark", N_("Solarized Dark")},
    {"solarized-light", N_("Solarized Light")},
    {"matrix", N_("Matrix")},
    {"one-half-dark", N_("One Half Dark")},
    {"one-half-light", N_("One Half Light")},
    {"monokai", N_("Monokai")},
};

typedef struct {
    int size_pts; // Font size in points
} MenuFontSizeEntry;

static const MenuFontSizeEntry FONT_SIZE_ENTRIES[] = {
    {9}, {10}, {11}, {12}, {13}, {14}, {15}, {16},
    {17}, {18}, {20}, {22}, {24}, {28}, {32}, {36}, {40}, {48}
};

typedef struct {
    NemoTerminalSyncMode mode;   // Sync mode enum value
    const gchar *label_pot;      // Translatable label
} MenuSyncModeEntry;

static const MenuSyncModeEntry LOCAL_SYNC_MODE_ENTRIES[] = {
    {NEMO_TERMINAL_SYNC_BOTH, N_("Sync Both Ways")},
    {NEMO_TERMINAL_SYNC_FM_TO_TERM, N_("Sync File Manager → Terminal")},
    {NEMO_TERMINAL_SYNC_TERM_TO_FM, N_("Sync Terminal → File Manager")},
    {NEMO_TERMINAL_SYNC_NONE, N_("No Sync")}
};

typedef struct {
    NemoTerminalSshAutoConnectMode mode; // Auto-connect mode enum
    const gchar *label_pot;              // Translatable label
    NemoTerminalSyncMode sync_mode_for_connection; // Sync mode to use if auto-connecting
} MenuSshAutoConnectEntry;

static const MenuSshAutoConnectEntry SFTP_AUTO_CONNECT_ENTRIES[] = {
    {NEMO_TERMINAL_SSH_AUTOCONNECT_OFF, N_("Do not connect automatically"), NEMO_TERMINAL_SYNC_NONE},
    {NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH, N_("Automatically connect and sync both ways"), NEMO_TERMINAL_SYNC_BOTH},
    {NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM, N_("Automatically connect and sync: File Manager → Terminal"), NEMO_TERMINAL_SYNC_FM_TO_TERM},
    {NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM, N_("Automatically connect and sync: Terminal → File Manager"), NEMO_TERMINAL_SYNC_TERM_TO_FM},
    {NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE, N_("Automatically connect without syncing"), NEMO_TERMINAL_SYNC_NONE}
};

static const MenuSyncModeEntry MANUAL_SSH_SYNC_ENTRIES[] = {
    {NEMO_TERMINAL_SYNC_BOTH, N_("Sync folder both ways")},
    {NEMO_TERMINAL_SYNC_FM_TO_TERM, N_("Sync folder from File Manager → Terminal")},
    {NEMO_TERMINAL_SYNC_TERM_TO_FM, N_("Sync folder from Terminal → File Manager")},
    {NEMO_TERMINAL_SYNC_NONE, N_("No folder sync")}
};

/**
 * _on_menu_item_activate_widget_action:
 * @menuitem: The #GtkMenuItem that was activated.
 * @user_data: Unused in this callback.
 *
 * Compatibility handler to bridge GtkMenuItem's "activate" signal to
 * a GAction registered on the widget. The widget instance and action name
 * are retrieved from data set on the menu item.
 */
static void
_on_menu_item_activate_widget_action (GtkMenuItem *menuitem,
                                      gpointer     user_data)
{
    // Retrieve the NemoTerminalWidget instance and the detailed action name stored on the menu item.
    NemoTerminalWidget *self = (NemoTerminalWidget *)g_object_get_data(G_OBJECT(menuitem), "ntw-self");
    const gchar *detailed_action_name = (const gchar *)g_object_get_data(G_OBJECT(menuitem), "ntw-action-name");

    if (self && detailed_action_name) {
        // Parse the action name from the detailed_action_name (e.g., "terminal.copy" -> "copy").
        // The detailed_action_name includes a prefix like "widget." or "terminal.".
        const gchar *dot = strchr(detailed_action_name, '.');
        if (dot && self->action_group) {
            const gchar *action_name = dot + 1;  // Get the action name part after the dot.
            g_action_group_activate_action(G_ACTION_GROUP(self->action_group), action_name, NULL);
        } else {
            g_warning("Could not parse action name or action group not found for menu item: %s", detailed_action_name);
        }
    }
}

/**
 * _add_action_menu_item_compat:
 * @menu_shell: The #GtkMenuShell to add the item to.
 * @self: The #NemoTerminalWidget instance (used for context in the action).
 * @label: The translatable label for the menu item.
 * @detailed_action_name: The full action name (e.g., "terminal.copy") to activate.
 *
 * Helper to create a #GtkMenuItem that, when activated, triggers a GAction
 * on the widget. This is for compatibility where direct GAction use in menus
 * might be problematic or for older GTK versions/styles.
 */
static void
_add_action_menu_item_compat(GtkMenuShell       *menu_shell,
                             NemoTerminalWidget *self,
                             const gchar        *label,
                             const gchar        *detailed_action_name)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    // Store necessary context on the GtkMenuItem itself for the callback.
    g_object_set_data(G_OBJECT(item), "ntw-self", self);
    g_object_set_data(G_OBJECT(item), "ntw-action-name", (gpointer)detailed_action_name); // Cast is okay for const gchar*
    g_signal_connect(item, "activate", G_CALLBACK(_on_menu_item_activate_widget_action), NULL); // user_data for signal is NULL
    gtk_menu_shell_append(menu_shell, item);
}

/**
 * _add_callback_menu_item:
 * @menu_shell: The #GtkMenuShell to add the item to.
 * @label: The translatable label for the menu item.
 * @callback: The GCallback function to invoke on activation.
 * @user_data: User data to pass to the callback.
 *
 * Helper to create a #GtkMenuItem that calls a specific C callback function
 * when activated.
 */
static void
_add_callback_menu_item(GtkMenuShell *menu_shell,
                        const gchar *label,
                        GCallback callback,
                        gpointer user_data)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", callback, user_data);
    gtk_menu_shell_append(menu_shell, item);
}

/**
 * _add_radio_menu_item_with_data:
 * @menu_shell: The #GtkMenuShell to add the item to.
 * @radio_group: Pointer to the GSList representing the radio group.
 * @label: The translatable label for the menu item.
 * @data_key: The key for attaching data_value to the item.
 * @data_value: The data to associate with the menu item (e.g., an enum or string).
 * @is_active: Whether this radio item should be initially active.
 * @activate_callback: The GCallback function to invoke on activation.
 * @user_data: User data to pass to the callback.
 *
 * Helper to create a #GtkRadioMenuItem, associate data with it,
 * and connect its "activate" signal.
 *
 * Returns: The created #GtkWidget (the radio menu item).
 */
static GtkWidget *
_add_radio_menu_item_with_data(GtkMenuShell *menu_shell,
                               GSList **radio_group,
                               const gchar *label,
                               const gchar *data_key,
                               gpointer data_value,
                               gboolean is_active,
                               GCallback activate_callback,
                               gpointer user_data)
{
    GtkWidget *item = gtk_radio_menu_item_new_with_label(*radio_group, label);
    if (*radio_group == NULL) { // First item in the group
        *radio_group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
    }

    g_object_set_data(G_OBJECT(item), data_key, data_value);

    if (is_active) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }
    g_signal_connect(item, "activate", activate_callback, user_data);
    gtk_menu_shell_append(menu_shell, item);
    return item;
}

/**
 * create_terminal_popup_menu:
 * @self: The #NemoTerminalWidget instance.
 *
 * Creates and populates the context menu for the terminal widget.
 * Includes options for copy/paste, color schemes, font sizes,
 * synchronization modes, and SSH connection management.
 *
 * Returns: A new #GtkWidget (the #GtkMenu). The caller does not own the GtkWidget.
 *          The menu will be shown via gtk_menu_popup.
 */
static GtkWidget *
create_terminal_popup_menu(NemoTerminalWidget *self)
{
    GtkWidget *menu, *menu_item, *submenu;
    gboolean is_sftp_location = FALSE;
    g_autofree gchar *current_uri = NULL;

    menu = gtk_menu_new();

    /* Standard Edit Actions: Copy, Paste, Select All */
    _add_action_menu_item_compat(GTK_MENU_SHELL(menu), self, _("Copy"), "terminal.copy");
    _add_action_menu_item_compat(GTK_MENU_SHELL(menu), self, _("Paste"), "terminal.paste");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    _add_action_menu_item_compat(GTK_MENU_SHELL(menu), self, _("Select All"), "terminal.select-all");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    /* Color Scheme Submenu */
    menu_item = gtk_menu_item_new_with_label(_("Color Scheme"));
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    GSList *color_scheme_radio_group = NULL;
    const gchar *current_scheme_val = nemo_terminal_widget_get_color_scheme(self);
    for (gsize i = 0; i < G_N_ELEMENTS(COLOR_SCHEME_ENTRIES); ++i) {
        _add_radio_menu_item_with_data(GTK_MENU_SHELL(submenu), &color_scheme_radio_group,
                                       _(COLOR_SCHEME_ENTRIES[i].label_pot),
                                       DATA_KEY_SCHEME_NAME, (gpointer)COLOR_SCHEME_ENTRIES[i].id,
                                       (g_strcmp0(current_scheme_val, COLOR_SCHEME_ENTRIES[i].id) == 0),
                                       G_CALLBACK(on_color_scheme_changed), self);
    }

    /* Font Size Submenu */
    menu_item = gtk_menu_item_new_with_label(_("Font Size"));
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    // Get current font size to pre-select the closest one in the menu
    g_autoptr(PangoFontDescription) current_font_desc = pango_font_description_copy(
        vte_terminal_get_font(self->terminal));
    int current_size_pts = pango_font_description_get_size(current_font_desc) / PANGO_SCALE;

    // Find the closest predefined font size to the current one
    int closest_size_idx = 0;
    if (G_N_ELEMENTS(FONT_SIZE_ENTRIES) > 0) {
        int min_diff = abs(FONT_SIZE_ENTRIES[0].size_pts - current_size_pts);
        for (gsize i = 1; i < G_N_ELEMENTS(FONT_SIZE_ENTRIES); i++) {
            int diff = abs(FONT_SIZE_ENTRIES[i].size_pts - current_size_pts);
            if (diff < min_diff) {
                min_diff = diff;
                closest_size_idx = i;
            }
        }
    }

    GSList *font_size_radio_group = NULL;
    for (gsize i = 0; i < G_N_ELEMENTS(FONT_SIZE_ENTRIES); ++i) {
        g_autofree gchar *label = g_strdup_printf("%d", FONT_SIZE_ENTRIES[i].size_pts);
        _add_radio_menu_item_with_data(GTK_MENU_SHELL(submenu), &font_size_radio_group,
                                       label, DATA_KEY_FONT_SIZE,
                                       GINT_TO_POINTER(FONT_SIZE_ENTRIES[i].size_pts),
                                       (i == closest_size_idx), // Check if this is the closest size
                                       G_CALLBACK(on_font_size_changed), self);
    }
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    // Determine if the current location is an SFTP path
    if (self->current_location != NULL && G_IS_FILE(self->current_location)) {
        current_uri = g_file_get_uri(self->current_location);
        if (current_uri && g_str_has_prefix(current_uri, "sftp://")) {
            is_sftp_location = TRUE;
        }
    } else if (self->current_location != NULL) {
        // This case should ideally not happen if current_location is always GFile or NULL
        g_warning("self->current_location is not a GFile in create_terminal_popup_menu");
    }

    /* Local Folder Sync Submenu (only shown if not in SSH mode) */
    if (!self->in_ssh_mode) {
        menu_item = gtk_menu_item_new_with_label(_("Local Folder Sync"));
        GtkWidget *local_sync_submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), local_sync_submenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

        GSList *local_sync_radio_group = NULL;
        for (gsize i = 0; i < G_N_ELEMENTS(LOCAL_SYNC_MODE_ENTRIES); ++i) {
            _add_radio_menu_item_with_data(GTK_MENU_SHELL(local_sync_submenu), &local_sync_radio_group,
                                           _(LOCAL_SYNC_MODE_ENTRIES[i].label_pot),
                                           DATA_KEY_LOCAL_SYNC_MODE, GINT_TO_POINTER(LOCAL_SYNC_MODE_ENTRIES[i].mode),
                                           (self->local_sync_mode == LOCAL_SYNC_MODE_ENTRIES[i].mode),
                                           G_CALLBACK(on_local_sync_mode_changed), self);
        }
    }

    /* SSH Auto-Connect Submenu (only shown if not in SSH mode) */
    if (!self->in_ssh_mode) {
        menu_item = gtk_menu_item_new_with_label(_("SSH Auto-Connect"));
        GtkWidget *sftp_auto_connect_submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), sftp_auto_connect_submenu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

        GSList *sftp_auto_radio_group = NULL;
        for (gsize i = 0; i < G_N_ELEMENTS(SFTP_AUTO_CONNECT_ENTRIES); ++i) {
            g_autofree gchar *label = g_strdup(_(SFTP_AUTO_CONNECT_ENTRIES[i].label_pot));
            GtkWidget *auto_item_widget = _add_radio_menu_item_with_data(
                                             GTK_MENU_SHELL(sftp_auto_connect_submenu), &sftp_auto_radio_group,
                                             label, DATA_KEY_SFTP_AUTO_CONNECT_MODE,
                                             GINT_TO_POINTER(SFTP_AUTO_CONNECT_ENTRIES[i].mode),
                                             (self->ssh_auto_connect_mode == SFTP_AUTO_CONNECT_ENTRIES[i].mode),
                                             G_CALLBACK(on_sftp_auto_connect_behavior_changed), self);

            // If current location is SFTP, attach its details to the auto-connect menu item
            // This allows immediate connection if an auto-connect option is chosen.
            if (is_sftp_location && self->current_location && G_IS_FILE(self->current_location) && auto_item_widget) {
                gchar *h = NULL, *u = NULL, *p = NULL;
                if (parse_gvfs_ssh_path(self->current_location, &h, &u, &p)) {
                    g_object_set_data_full(G_OBJECT(auto_item_widget), DATA_KEY_SSH_HOSTNAME, h, (GDestroyNotify)g_free);
                    g_object_set_data_full(G_OBJECT(auto_item_widget), DATA_KEY_SSH_USERNAME, u, (GDestroyNotify)g_free);
                    g_object_set_data_full(G_OBJECT(auto_item_widget), DATA_KEY_SSH_PORT, p, (GDestroyNotify)g_free);
                } else {
                    // Free if parse_gvfs_ssh_path allocated them but returned FALSE
                    g_free(h); g_free(u); g_free(p);
                }
            }
        }
    }

    /* SSH Connection Management: Disconnect (if in SSH) or Manual Connect (if on SFTP path) */
    if (self->in_ssh_mode) {
        // Option to disconnect from the current SSH session
        _add_callback_menu_item(GTK_MENU_SHELL(menu), _("Disconnect from SSH"),
                                G_CALLBACK(on_ssh_exit_activate), self);
    } else if (is_sftp_location && self->current_location && G_IS_FILE(self->current_location)) {
        // Option to manually connect to the current SFTP location via SSH
        gchar *hostname = NULL, *username = NULL, *port = NULL;
        gboolean can_connect_ssh = parse_gvfs_ssh_path(self->current_location, &hostname, &username, &port);

        if (can_connect_ssh && hostname != NULL) {
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

            // Build a descriptive label for the SSH connection submenu
            g_autofree GString *label_gstr = g_string_new(_("SSH Connection to "));
            if (username != NULL && *username != '\0') {
                g_string_append_printf(label_gstr, "%s@", username);
            }
            g_string_append(label_gstr, hostname);
            if (port != NULL && *port != '\0') {
                g_string_append_printf(label_gstr, ":%s", port);
            }

            menu_item = gtk_menu_item_new_with_label(label_gstr->str);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

            // Submenu for choosing sync mode for manual SSH connection
            GtkWidget *ssh_manual_connect_menu = gtk_menu_new();
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), ssh_manual_connect_menu);

            for (gsize i = 0; i < G_N_ELEMENTS(MANUAL_SSH_SYNC_ENTRIES); ++i) {
                GtkWidget *sync_item = gtk_menu_item_new_with_label(_(MANUAL_SSH_SYNC_ENTRIES[i].label_pot));
                // Store SSH details on the menu item for the callback
                g_object_set_data_full(G_OBJECT(sync_item), DATA_KEY_SSH_HOSTNAME, g_strdup(hostname), (GDestroyNotify)g_free);
                g_object_set_data_full(G_OBJECT(sync_item), DATA_KEY_SSH_USERNAME, g_strdup(username), (GDestroyNotify)g_free);
                g_object_set_data_full(G_OBJECT(sync_item), DATA_KEY_SSH_PORT, g_strdup(port), (GDestroyNotify)g_free);
                g_object_set_data(G_OBJECT(sync_item), DATA_KEY_SSH_SYNC_MODE, GINT_TO_POINTER(MANUAL_SSH_SYNC_ENTRIES[i].mode));
                g_signal_connect(sync_item, "activate", G_CALLBACK(on_ssh_connect_activate), self);
                gtk_menu_shell_append(GTK_MENU_SHELL(ssh_manual_connect_menu), sync_item);
            }
        }
        g_free(hostname); g_free(username); g_free(port);
    }

    gtk_widget_show_all(menu);
    return menu;
}


/**
 * change_directory_in_terminal:
 * @self: The #NemoTerminalWidget instance.
 * @location: The #GFile representing the new directory.
 *
 * Changes the current working directory in the VTE terminal to match the
 * provided @location. This function respects the configured synchronization
 * mode (local or SSH) and only performs the `cd` if synchronization from
 * File Manager to Terminal is enabled.
 */
static void
change_directory_in_terminal(NemoTerminalWidget *self, GFile *location)
{
    g_autofree char *path = NULL;
    gboolean should_sync = TRUE; // Assume sync unless checks determine otherwise

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    g_return_if_fail(location != NULL && G_IS_FILE(location));

    /* Determine if a 'cd' command should be sent based on current mode and sync settings */
    if (self->in_ssh_mode)
    {
        // In SSH mode, sync from FM to Terminal requires SYNC_BOTH or SYNC_FM_TO_TERM
        if (self->ssh_sync_mode != NEMO_TERMINAL_SYNC_BOTH &&
            self->ssh_sync_mode != NEMO_TERMINAL_SYNC_FM_TO_TERM)
        {
            should_sync = FALSE;
        }

        if (should_sync)
        {
            // For SSH, get the remote path from the SFTP GFile
            path = get_remote_path_from_sftp_gfile(location);
            if (!path)
            {
                g_warning("Failed to get remote path for SSH cd from GFile URI: %s",
                          g_file_peek_path(location) ? g_file_peek_path(location) : "(unknown)");
            }
            else if (path[0] == '\0') // Empty path typically means root
            {
                g_free(path);
                path = g_strdup("/");
            }
        }
    }
    else // Not in SSH mode (local terminal)
    {
        // For local terminal, sync from FM to Terminal requires SYNC_BOTH or SYNC_FM_TO_TERM
        if (self->local_sync_mode != NEMO_TERMINAL_SYNC_BOTH &&
            self->local_sync_mode != NEMO_TERMINAL_SYNC_FM_TO_TERM)
        {
            should_sync = FALSE;
        }
    }

    if (!should_sync)
    {
        return; // Sync not enabled for this direction
    }

    // If not in SSH mode and sync is enabled, get the local file path
    if (!self->in_ssh_mode) // Implies should_sync is TRUE here for local
    {
        if (g_file_query_exists(location, NULL))
        {
            path = g_file_get_path(location);
        }
        else
        {
            // Target location doesn't exist, warn and abort cd
            g_autofree gchar *uri_for_warning = g_file_get_uri(location);
            g_warning("Target local location %s for cd no longer exists. Aborting cd.",
                      uri_for_warning ? uri_for_warning : "(unknown URI)");
            path = NULL; // Ensure path is NULL so no cd command is fed
        }
    }

    // If a valid path was determined, feed the cd command
    if (path != NULL && *path != '\0') // Ensure path is not NULL or empty
    {
        // Tell terminal to ignore its own "directory-changed" signal for this explicit cd
        self->ignore_next_terminal_cd_signal = TRUE;
        feed_cd_command(VTE_TERMINAL(self->terminal), path);
    }
    else if (should_sync) // Path is NULL but we intended to sync
    {
        g_warning("Path for cd command is NULL, aborting cd. Location: %s",
                  g_file_peek_path(location) ? g_file_peek_path(location) : "(unknown)");
    }
}

/**
 * get_remote_path_from_sftp_gfile:
 * @location: A #GFile, presumably an SFTP location.
 *
 * Extracts the absolute remote path from an SFTP #GFile.
 * It first tries to parse the URI (e.g., "sftp://host/path").
 * As a fallback for GVFS-mounted SFTP locations that might appear as local
 * file paths (e.g., "/run/user/UID/gvfs/sftp:host=.../remote/path"),
 * it attempts to parse these paths.
 *
 * Returns: A newly allocated string containing the remote path (e.g., "/home/user/docs"),
 *          or "/" if the path component is empty. Returns %NULL on failure.
 *          The caller must free the returned string.
 */
static gchar *
get_remote_path_from_sftp_gfile(GFile *location)
{
    g_return_val_if_fail(G_IS_FILE(location), NULL);

    gchar *remote_path = NULL;
    g_autofree gchar *uri = g_file_get_uri(location);

    if (uri && g_str_has_prefix(uri, "sftp://"))
    {
        // Unescape the URI to handle special characters in path components
        g_autofree gchar *decoded_uri = g_uri_unescape_string(uri, NULL);
        if (decoded_uri) {
            // Find the path part: sftp://[userinfo@]host[:port]/path
            // Add 7 to skip "sftp://".
            const char *host_part_end = strstr(decoded_uri + 7, "/");
            if (host_part_end) { // Found a '/' after the host part
                remote_path = g_strdup(host_part_end);
            } else { // No path component after host, implies root directory on server
                remote_path = g_strdup("/");
            }
        }
    }
    else // Fallback: Try to parse as a GVFS local mount path for SFTP
    {
        g_autofree gchar *path = g_file_get_path(location);
        // Example GVFS path: /run/user/1000/gvfs/sftp:host=example.com,user=myuser/actual/remote/path
        // We need to extract "/actual/remote/path"
        if (path && g_str_has_prefix(path, "/run/user/") && strstr(path, "/gvfs/sftp:host="))
        {
            // Find the start of the GVFS SFTP details part
            char *sftp_details_part = strstr(path, "/gvfs/sftp:host=");
            if (sftp_details_part)
            {
                // The actual remote path starts after the GVFS connection string part.
                // e.g. "sftp:host=...,user=..." or "sftp:host=..."
                // The first '/' after this connection string segment marks the start of the remote path.
                char *path_start = strchr(sftp_details_part + strlen("/gvfs/"), '/'); // Search for '/' after "/gvfs/"
                if (path_start)
                {
                    remote_path = g_strdup(path_start);
                }
                else // No further '/' means it's the root of the SFTP mount
                {
                    remote_path = g_strdup("/");
                }
            }
        }
    }
    return remote_path;
}

/**
 * setup_terminal_font:
 * @terminal: The #VteTerminal widget.
 *
 * Configures the font for the VTE terminal. It uses the system's monospace
 * font setting ("org.gnome.desktop.interface monospace-font-name") and
 * a font size retrieved via `nemo_terminal_get_font_size()`.
 */
static void
setup_terminal_font(VteTerminal *terminal)
{
    g_autoptr(PangoFontDescription) font_desc = NULL;
    g_autoptr(GSettings) interface_settings = NULL;
    g_autofree gchar *font_name = NULL;
    int font_size_pts;

    // Get system monospace font name
    interface_settings = g_settings_new("org.gnome.desktop.interface");
    font_name = g_settings_get_string(interface_settings, "monospace-font-name");

    // Get saved/default font size for the terminal
    font_size_pts = nemo_terminal_get_font_size();

    if (font_name && *font_name)
    {
        font_desc = pango_font_description_from_string(font_name);
    }
    else // Fallback if system font name is not set
    {
        font_desc = pango_font_description_new();
        pango_font_description_set_family(font_desc, "Monospace"); // Default to generic "Monospace"
    }

    pango_font_description_set_size(font_desc, font_size_pts * PANGO_SCALE);
    vte_terminal_set_font(terminal, font_desc);
    // VTE terminal takes its own copy of font_desc, so we can free ours.
}

/**
 * focus_once_and_remove:
 * @user_data: The #GtkWidget (VTE terminal) to focus.
 *
 * Idle callback to grab focus for the terminal widget.
 * Removes itself after execution. Used to ensure focus is set
 * after other UI events might have settled.
 *
 * Returns: %G_SOURCE_REMOVE to ensure it runs only once.
 */
static gboolean
focus_once_and_remove(gpointer user_data)
{
    GtkWidget *widget_to_focus = GTK_WIDGET(user_data);
    NemoTerminalWidget *self;

    if (GTK_IS_WIDGET(widget_to_focus) && gtk_widget_get_window(widget_to_focus)) { // Ensure widget is realized
        gtk_widget_grab_focus(widget_to_focus);
    }

    // Clear the timeout ID from the parent NemoTerminalWidget
    self = NEMO_TERMINAL_WIDGET(gtk_widget_get_ancestor(widget_to_focus, NEMO_TYPE_TERMINAL_WIDGET));
    if (self && self->focus_timeout_id > 0) // Check if self is valid and ID matches
    {
        // This function is called by the timeout, so we can't remove by ID here.
        // Instead, the source removes itself. We just clear our record.
        self->focus_timeout_id = 0;
    }
    return G_SOURCE_REMOVE;
}

/**
 * reset_toggling_flag:
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Timeout callback to reset the `in_toggling` flag. This acts as a
 * debounce mechanism for visibility toggling actions.
 *
 * Returns: %G_SOURCE_REMOVE to ensure it runs only once.
 */
static gboolean
reset_toggling_flag(gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    if (NEMO_IS_TERMINAL_WIDGET(self)) {
        self->in_toggling = FALSE;
    }
    return G_SOURCE_REMOVE;
}

/**
 * apply_initial_size_idle:
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Idle callback to apply the terminal's saved height. This is typically
 * called after the widget and its container paned are realized, ensuring
 * dimensions are available.
 *
 * Returns: %G_SOURCE_REMOVE to ensure it runs only once.
 */
static gboolean
apply_initial_size_idle(gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    if (NEMO_IS_TERMINAL_WIDGET(self)) {
        nemo_terminal_widget_apply_new_size(self);
    }
    return G_SOURCE_REMOVE;
}

/**
 * on_terminal_key_press:
 * @widget: The #VteTerminal widget where the key press occurred.
 * @event: The #GdkEventKey for the key press.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Handles key press events within the terminal, primarily for implementing
 * custom keyboard shortcuts (e.g., F4 to toggle visibility, Ctrl+Shift+C/V/A
 * for copy/paste/select all, Ctrl+Shift+S for SSH connect).
 *
 * Returns: %TRUE if the event was handled, %FALSE otherwise.
 */
static gboolean
on_terminal_key_press(GtkWidget *widget,
                      GdkEventKey *event,
                      gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    guint keyval = event->keyval;
    GdkModifierType state = event->state; // Use GdkEventKey->state for modifiers

    /* F4: Toggle terminal visibility */
    if (keyval == GDK_KEY_F4)
    {
        nemo_terminal_widget_toggle_visible(self);
        return TRUE; // Event handled
    }

    /* Standard terminal shortcuts (Ctrl+Shift+Letter) */
    if ((state & GDK_CONTROL_MASK) && (state & GDK_SHIFT_MASK))
    {
        switch (keyval)
        {
            case GDK_KEY_C: // Ctrl+Shift+C for Copy
            case GDK_KEY_c:
                vte_terminal_copy_clipboard_format(self->terminal, VTE_FORMAT_TEXT);
                return TRUE;

            case GDK_KEY_V: // Ctrl+Shift+V for Paste
            case GDK_KEY_v:
                vte_terminal_paste_clipboard(self->terminal);
                return TRUE;

            case GDK_KEY_A: // Ctrl+Shift+A for Select All
            case GDK_KEY_a:
                vte_terminal_select_all(self->terminal);
                return TRUE;

            case GDK_KEY_S: // Ctrl+Shift+S for SSH connect (if on SFTP path)
            case GDK_KEY_s:
                if (self->current_location && G_IS_FILE(self->current_location) && !self->in_ssh_mode)
                {
                    g_autofree gchar *hostname = NULL;
                    g_autofree gchar *username = NULL;
                    g_autofree gchar *port = NULL;

                    if (parse_gvfs_ssh_path(self->current_location, &hostname, &username, &port))
                    {
                        // Default to SYNC_BOTH for keyboard shortcut initiated connections
                        _initiate_ssh_connection(self, hostname, username, port, NEMO_TERMINAL_SYNC_BOTH);
                        // hostname, username, port are freed by g_autofree
                        return TRUE;
                    }
                }
                break;
        }
    }
    return FALSE; // Event not handled by this function
}

/**
 * on_terminal_directory_changed:
 * @terminal: The #VteTerminal whose directory changed.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for VTE's "current-directory-uri-changed" (or equivalent) signal.
 * When the terminal's CWD changes (e.g., user types `cd`), this function
 * updates the file manager's location if synchronization from
 * Terminal to File Manager is enabled.
 */
static void
on_terminal_directory_changed(VteTerminal *terminal,
                              gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    const char *cwd_uri = vte_terminal_get_current_directory_uri(terminal);
    g_autoptr(GFile) new_gfile_location = NULL;
    gboolean should_sync_to_fm = TRUE; // Assume sync unless checks determine otherwise

    if (!cwd_uri) return; // No CWD URI available

    // If ignore_next_terminal_cd_signal is set, it means this change was
    // programmatically triggered (e.g., by FM changing location).
    // We should update our internal current_location if it's a local shell
    // to reflect this, but not sync back to FM.
    if (self->ignore_next_terminal_cd_signal) {
        self->ignore_next_terminal_cd_signal = FALSE;
        if (!self->in_ssh_mode) { // Only update self->current_location for local shell initial cd
            g_autoptr(GFile) temp_gfile = g_file_new_for_uri(cwd_uri);
            if (temp_gfile) {
                if (!self->current_location || !g_file_equal(temp_gfile, self->current_location)) {
                    g_set_object(&self->current_location, temp_gfile);
                    // No g_signal_emit here, as this is an internal sync from an explicit cd.
                }
            }
        }
        return;
    }

    /* Determine if a sync to File Manager should occur */
    if (self->in_ssh_mode)
    {
        // In SSH mode, sync from Term to FM requires SYNC_BOTH or SYNC_TERM_TO_FM
        if (self->ssh_sync_mode != NEMO_TERMINAL_SYNC_BOTH &&
            self->ssh_sync_mode != NEMO_TERMINAL_SYNC_TERM_TO_FM)
        {
            should_sync_to_fm = FALSE;
        }

        if (should_sync_to_fm && g_str_has_prefix(cwd_uri, "file://")) // VTE gives local file:// URI
        {
            // Convert local path from terminal (e.g., /home/user) to an SFTP URI
            g_autofree gchar *local_path_from_uri = g_filename_from_uri(cwd_uri, NULL, NULL);
            if (local_path_from_uri && self->ssh_hostname) // Need hostname to build SFTP URI
            {
                g_autofree GString *sftp_uri_builder = g_string_new("sftp://");
                if (self->ssh_username && *self->ssh_username) {
                    g_string_append_printf(sftp_uri_builder, "%s@", self->ssh_username);
                }
                g_string_append(sftp_uri_builder, self->ssh_hostname);
                if (self->ssh_port && *self->ssh_port) {
                    g_string_append_printf(sftp_uri_builder, ":%s", self->ssh_port);
                }
                // Ensure path starts with '/', g_filename_from_uri should provide absolute path
                g_string_append(sftp_uri_builder, local_path_from_uri);
                new_gfile_location = g_file_new_for_uri(sftp_uri_builder->str);
            }
        }
    }
    else // Not in SSH mode (local terminal)
    {
        // For local terminal, sync from Term to FM requires SYNC_BOTH or SYNC_TERM_TO_FM
        if (self->local_sync_mode != NEMO_TERMINAL_SYNC_BOTH &&
            self->local_sync_mode != NEMO_TERMINAL_SYNC_TERM_TO_FM)
        {
            should_sync_to_fm = FALSE;
        }
        if (should_sync_to_fm)
        {
            new_gfile_location = g_file_new_for_uri(cwd_uri);
        }
    }

    if (!should_sync_to_fm || !new_gfile_location)
    {
        return; // Sync not enabled, or failed to create GFile for the new location
    }

    // If the new location is different from the current one, update and emit signal
    if (!self->current_location || !g_file_equal(new_gfile_location, self->current_location))
    {
        g_set_object(&self->current_location, new_gfile_location); // Updates ref count
        g_signal_emit_by_name(self, "change-directory", new_gfile_location);

        // If terminal had focus, try to maintain it after the directory change potentially re-renders UI
        if (self->maintain_focus && gtk_widget_has_focus(GTK_WIDGET(self->terminal)))
        {
            if (self->focus_timeout_id > 0) // Cancel any pending focus attempt
            {
                g_source_remove(self->focus_timeout_id);
            }
            // Schedule a new focus attempt
            self->focus_timeout_id = g_timeout_add(50, focus_once_and_remove, GTK_WIDGET(self->terminal));
        }
    }
    // new_gfile_location is unref'd by g_autoptr when it goes out of scope
}

/**
 * on_terminal_child_exited:
 * @terminal: The #VteTerminal whose child process exited.
 * @status: The exit status of the child process.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for VTE's "child-exited" signal.
 * SSH session termination (e.g., connection drop, remote exit) and
 * local shell exits.
 * If an SSH session ends unexpectedly, it resets to a local terminal.
 * If a local shell exits, it may respawn based on visibility.
 */
static void
on_terminal_child_exited(VteTerminal *terminal,
                         gint status,
                         gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    // If we are explicitly exiting SSH (e.g., user clicked "Disconnect"),
    // on_ssh_exit_activate handles the reset. Avoid double reset.
    if (self->is_exiting_ssh) {
        return;
    }

    if (self->in_ssh_mode) {
        // The shell process hosting the SSH client has exited. This could be due to
        // network issues, the SSH client itself terminating, or the remote end closing.
        g_warning("Shell hosting SSH session exited unexpectedly (status %d). Resetting to local terminal.", status);
        self->is_exiting_ssh = TRUE; // Prevent re-entry during reset
        reset_terminal_to_current_location(self); // Clears SSH state, spawns local shell
        self->is_exiting_ssh = FALSE;
    } else {
        // Local shell exited.
        // Respawn if the terminal widget is part of a window and is currently visible,
        // or mark for respawn if it's hidden.
        if (GTK_IS_WIDGET(self) && gtk_widget_get_ancestor(GTK_WIDGET(self), GTK_TYPE_WINDOW)) {
            if (self->is_visible) {
                 spawn_terminal_in_widget(self); // Respawn local shell
            } else {
                 self->needs_respawn = TRUE; // Mark to respawn when next shown
            }
        }
    }
}

/**
 * on_container_size_changed:
 * @paned: The #GtkPaned widget whose position (divider) changed.
 * @pspec: The #GParamSpec of the property that changed (unused).
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for the "notify::position" signal on the GtkPaned that
 * contains the terminal. Saves the new height of the terminal pane
 * when the user resizes it.
 */
static void
on_container_size_changed(GtkPaned *paned,
                          GParamSpec *pspec,
                          gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    // Basic validation
    if (!self || !NEMO_IS_TERMINAL_WIDGET(self) || !GTK_IS_PANED(paned)) return;
    if (!gtk_widget_get_realized(GTK_WIDGET(paned))) return; // Avoid acting on unrealized widgets

    int position = gtk_paned_get_position(paned); // Position of the divider
    int total_height = gtk_widget_get_allocated_height(GTK_WIDGET(paned));

    if (total_height <= 0) return; // Avoid division by zero or negative heights

    // For a GtkPaned with vertical orientation:
    // Child1 (top) height = position
    // Child2 (bottom, our terminal) height = total_height - position
    int terminal_height = total_height - position;

    nemo_terminal_widget_save_height(self, terminal_height);
}

/**
 * on_terminal_button_press:
 * @widget: The #VteTerminal widget where the button press occurred.
 * @event: The #GdkEventButton for the button press.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Handles button press events in the terminal, primarily to show the
 * context menu on a secondary (right) click.
 *
 * Returns: %TRUE if the event was handled (menu shown), %FALSE otherwise.
 */
static gboolean
on_terminal_button_press(GtkWidget *widget,
                         GdkEventButton *event,
                         gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    // Show context menu on right-click (button 3, or secondary button)
    if (event->button == GDK_BUTTON_SECONDARY && event->type == GDK_BUTTON_PRESS)
    {
        GtkWidget *menu = create_terminal_popup_menu(self);
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                       event->button, event->time);
        return TRUE;
    }
    return FALSE;
}

/**
 * on_copy_activate:
 * @action: The "copy" #GSimpleAction that was activated.
 * @parameter: (Unused) Parameters for the action.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Action handler for "copy". Copies selected text from the terminal to the clipboard.
 */
static void
on_copy_activate(GSimpleAction *action,
                 GVariant *parameter,
                 gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(self->terminal), VTE_FORMAT_TEXT);
}

/**
 * on_paste_activate:
 * @action: The "paste" #GSimpleAction that was activated.
 * @parameter: (Unused) Parameters for the action.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Action handler for "paste". Pastes text from the clipboard into the terminal.
 */
static void
on_paste_activate(GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_paste_clipboard(self->terminal);
}

/**
 * on_select_all_activate:
 * @action: The "select-all" #GSimpleAction that was activated.
 * @parameter: (Unused) Parameters for the action.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Action handler for "select-all". Selects all text in the terminal.
 */
static void
on_select_all_activate(GSimpleAction *action,
                       GVariant *parameter,
                       gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    vte_terminal_select_all(self->terminal);
}

/**
 * on_font_size_changed:
 * @widget: The #GtkRadioMenuItem for font size that was activated.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback when a font size is selected from the context menu.
 * Updates the terminal's font size and saves the setting.
 */
static void
on_font_size_changed(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    // Only act if the radio item is being activated (not deactivated)
    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
        return;

    gpointer size_data = g_object_get_data(G_OBJECT(widget), DATA_KEY_FONT_SIZE);
    if (size_data != NULL)
    {
        int font_size_pts = GPOINTER_TO_INT(size_data);

        g_autoptr(PangoFontDescription) font_desc = pango_font_description_copy(
            vte_terminal_get_font(self->terminal));

        pango_font_description_set_size(font_desc, font_size_pts * PANGO_SCALE);
        vte_terminal_set_font(self->terminal, font_desc);
        nemo_terminal_widget_save_font_size(self, font_size_pts); // Save the setting
    }
}

/**
 * on_color_scheme_changed:
 * @widget: The #GtkRadioMenuItem for color scheme that was activated.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback when a color scheme is selected from the context menu.
 * Sets the new color scheme for the terminal.
 */
static void
on_color_scheme_changed(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
        return;

    const gchar *scheme_name = g_object_get_data(G_OBJECT(widget), DATA_KEY_SCHEME_NAME);
    if (scheme_name != NULL)
    {
        nemo_terminal_widget_set_color_scheme(self, scheme_name);
    }
}

/**
 * on_local_sync_mode_changed:
 * @widget: The #GtkRadioMenuItem for local sync mode that was activated.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback when the local folder synchronization mode is changed via the menu.
 * Updates the widget's state, saves the setting, and may respawn the terminal
 * to apply new PROMPT_COMMAND if needed.
 */
static void
on_local_sync_mode_changed(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
        return;

    gpointer mode_data = g_object_get_data(G_OBJECT(widget), DATA_KEY_LOCAL_SYNC_MODE);
    NemoTerminalSyncMode new_mode = GPOINTER_TO_INT(mode_data);

    if (self->local_sync_mode == new_mode) return; // No change

    self->local_sync_mode = new_mode;
    g_settings_set_enum(nemo_window_state, "local-terminal-sync-mode", new_mode);

    // If not in SSH mode, changing local sync settings might require respawning
    // the terminal to update PROMPT_COMMAND for OSC7.
    if (!self->in_ssh_mode)
    {
        // Respawn to ensure PROMPT_COMMAND is correctly set/unset for OSC7.
        // This provides immediate feedback of the new sync mode.
        spawn_terminal_in_widget(self);
    }
}

/**
 * on_sftp_auto_connect_behavior_changed:
 * @widget: The #GtkRadioMenuItem for SSH auto-connect behavior that was activated.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback when the SFTP/SSH auto-connect behavior is changed via the menu.
 * Updates widget state, saves the setting, and may initiate an SSH connection
 * if an auto-connect option is chosen and the current location is SFTP.
 */
static void
on_sftp_auto_connect_behavior_changed(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    if (!gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
        return;

    gpointer mode_data = g_object_get_data(G_OBJECT(widget), DATA_KEY_SFTP_AUTO_CONNECT_MODE);
    NemoTerminalSshAutoConnectMode new_auto_mode = GPOINTER_TO_INT(mode_data);

    if (self->ssh_auto_connect_mode == new_auto_mode) return; // No change

    self->ssh_auto_connect_mode = new_auto_mode;
    g_settings_set_enum(nemo_window_state, "ssh-terminal-auto-connect-mode", new_auto_mode);

    // If an auto-connect option was selected (not "OFF") and we are not already in SSH:
    if (new_auto_mode != NEMO_TERMINAL_SSH_AUTOCONNECT_OFF && !self->in_ssh_mode)
    {
        // Retrieve SSH connection details stored on the menu item.
        // These would have been populated if the current location was SFTP when the menu was built.
        const gchar *hostname = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_HOSTNAME);
        const gchar *username = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_USERNAME);
        const gchar *port     = g_object_get_data(G_OBJECT(widget), DATA_KEY_SSH_PORT);

        if (hostname) // Hostname is essential for connection
        {
            NemoTerminalSyncMode sync_mode_for_connection;
            // Determine the sync mode based on the chosen auto-connect behavior
            switch (new_auto_mode)
            {
            case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH:
                sync_mode_for_connection = NEMO_TERMINAL_SYNC_BOTH;
                break;
            case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM:
                sync_mode_for_connection = NEMO_TERMINAL_SYNC_FM_TO_TERM;
                break;
            case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM:
                sync_mode_for_connection = NEMO_TERMINAL_SYNC_TERM_TO_FM;
                break;
            case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE:
                sync_mode_for_connection = NEMO_TERMINAL_SYNC_NONE;
                break;
            default: // Should not happen
                g_warning("Unexpected SSH auto-connect mode for immediate connection: %d", new_auto_mode);
                return;
            }
            _initiate_ssh_connection(self, hostname, username, port, sync_mode_for_connection);
        }
        // If hostname is NULL, it implies the menu was likely opened on a non-SFTP path,
        // so no immediate connection is attempted. The setting is saved for future SFTP navigation.
    }
}

/**
 * nemo_terminal_widget_class_init:
 * @klass: The #NemoTerminalWidgetClass to initialize.
 *
 * GObject class initialization function. Sets up signals and properties for the widget.
 */
static void
nemo_terminal_widget_class_init(NemoTerminalWidgetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GParamFlags flags;

    /* Signals */
    signals[CHANGE_DIRECTORY] =
        g_signal_new("change-directory",         // Signal name
                     G_TYPE_FROM_CLASS(klass),   // Owner class type
                     G_SIGNAL_RUN_LAST,          // Default emission stage
                     0,                          // Class offset (0 for default)
                     NULL, NULL,                 // Accumulator and marshaller data
                     g_cclosure_marshal_VOID__OBJECT, // Default C marshaller (void function, object param)
                     G_TYPE_NONE,                // Return type
                     1,                          // Number of parameters
                     G_TYPE_FILE);               // Parameter 1 type: GFile*

    signals[TOGGLE_VISIBILITY] =
        g_signal_new("toggle-visibility",
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__BOOLEAN, // Default C marshaller (void function, boolean param)
                     G_TYPE_NONE,
                     1,
                     G_TYPE_BOOLEAN);            // Parameter 1 type: gboolean

    /* Properties */
    flags = G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY;

    properties[PROP_CURRENT_LOCATION] =
        g_param_spec_object("current-location",     // Property name
                            "Current Location",       // Nickname
                            "The GFile representing the current directory.", // Blurb
                            G_TYPE_FILE,              // Property type
                            flags);                   // Property flags

    g_object_class_install_property(object_class, PROP_CURRENT_LOCATION, properties[PROP_CURRENT_LOCATION]);

    /* Override finalize method */
    object_class->finalize = nemo_terminal_widget_finalize;
}

/**
 * nemo_terminal_widget_init:
 * @self: The #NemoTerminalWidget instance to initialize.
 *
 * GObject instance initialization function. Sets up the widget's internal
 * structure, VTE terminal, default settings, and connects signals.
 */
static void
nemo_terminal_widget_init(NemoTerminalWidget *self)
{
    GtkStyleContext *context;
    GtkCssProvider *provider;

    // Initialize widget members
    self->scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(self->scrolled_window, TRUE);
    gtk_widget_set_hexpand(self->scrolled_window, TRUE);

    self->terminal = VTE_TERMINAL(vte_terminal_new());

    // SSH indicator label
    self->ssh_indicator = gtk_label_new("SSH");
    gtk_widget_set_name(self->ssh_indicator, "ssh-indicator"); // For CSS styling
    gtk_widget_set_no_show_all(self->ssh_indicator, TRUE); // Initially hidden
    gtk_widget_hide(self->ssh_indicator);
    gtk_widget_set_vexpand(self->ssh_indicator, FALSE);
    gtk_widget_set_hexpand(self->ssh_indicator, TRUE); // Allow to expand horizontally
    gtk_label_set_xalign(GTK_LABEL(self->ssh_indicator), 0.5); // Center the text

    // Apply CSS to SSH indicator
    provider = gtk_css_provider_new();
    // Basic styling for the SSH indicator label
    const char *css = "label#ssh-indicator { background-color: #3465a4; color: white; padding: 2px 5px; margin: 0; font-weight: bold; }";
    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    context = gtk_widget_get_style_context(self->ssh_indicator);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider); // Provider is now managed by style context

    // Initialize state flags
    self->is_exiting_ssh = FALSE;
    self->ignore_next_terminal_cd_signal = FALSE;
    self->container_paned = NULL; // Will be set when integrated into UI
    self->is_visible = FALSE; // Assume initially not visible until ensure_state
    self->needs_respawn = FALSE;
    self->in_toggling = FALSE;
    self->focus_timeout_id = 0;
    self->maintain_focus = TRUE; // Default to maintaining focus

    // Configure VTE terminal properties
    vte_terminal_set_scroll_on_output(self->terminal, FALSE);
    vte_terminal_set_scroll_on_keystroke(self->terminal, TRUE);
    vte_terminal_set_scrollback_lines(self->terminal, 10000); // Generous scrollback
    vte_terminal_set_allow_bold(self->terminal, TRUE);
    // vte_terminal_set_mouse_autohide(self->terminal, TRUE); // Optional: auto-hide mouse cursor

    setup_terminal_font(self->terminal); // Set font based on settings
    // Color scheme will be applied after self->color_scheme is initialized from settings
    // nemo_terminal_widget_apply_color_scheme(self); // Deferred until color_scheme is loaded

    // Connect VTE terminal signals
    g_signal_connect(self->terminal, "child-exited", G_CALLBACK(on_terminal_child_exited), self);
    g_signal_connect(self->terminal, "button-press-event", G_CALLBACK(on_terminal_button_press), self);
    g_signal_connect(self->terminal, "contents-changed", G_CALLBACK(on_terminal_contents_changed), self);

    // VTE signal for directory change can have different names in different versions
    if (g_signal_lookup("current-directory-uri-changed", VTE_TYPE_TERMINAL)) {
        g_signal_connect(self->terminal, "current-directory-uri-changed", G_CALLBACK(on_terminal_directory_changed), self);
    } else if (g_signal_lookup("directory-uri-changed", VTE_TYPE_TERMINAL)) { // Older VTE versions
        g_signal_connect(self->terminal, "directory-uri-changed", G_CALLBACK(on_terminal_directory_changed), self);
    } else {
        g_warning("Could not find a suitable directory change signal for VteTerminal.");
    }

    // Layout: VBox contains SSH indicator (optional) and ScrolledWindow (for terminal)
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox), self->ssh_indicator, FALSE, FALSE, 0); // Indicator at the top, no expand
    gtk_container_add(GTK_CONTAINER(self->scrolled_window), GTK_WIDGET(self->terminal));
    gtk_box_pack_start(GTK_BOX(vbox), self->scrolled_window, TRUE, TRUE, 0); // Scrolled window takes remaining space

    // Add the VBox to this NemoTerminalWidget (which is a GtkBox itself)
    gtk_box_pack_start(GTK_BOX(self), vbox, TRUE, TRUE, 0);

    // Load initial settings for sync modes and color scheme
    self->color_scheme = NULL; // Will be loaded by get_color_scheme on demand
    nemo_terminal_widget_get_color_scheme(self); // Ensure it's loaded
    nemo_terminal_widget_apply_color_scheme(self); // Apply the loaded scheme

    self->ssh_sync_mode = NEMO_TERMINAL_SYNC_NONE; // Default for new SSH sessions, can be overridden
    self->ssh_auto_connect_mode = g_settings_get_enum(nemo_window_state, "ssh-terminal-auto-connect-mode");
    self->local_sync_mode = g_settings_get_enum(nemo_window_state, "local-terminal-sync-mode");

    // Event handling for key presses (also on scrolled window for focus reasons)
    gtk_widget_set_can_focus(GTK_WIDGET(self->terminal), TRUE); // VTE terminal itself should be focusable
    gtk_widget_set_can_focus(self->scrolled_window, FALSE);    // Scrolled window usually not directly focusable
                                                               // but key events might bubble.
    // Connect key press to terminal primarily, and to self (the GtkBox) as a fallback if needed.
    // Or, let key events propagate from terminal. This seems fine for now.
    g_signal_connect(self->terminal, "key-press-event", G_CALLBACK(on_terminal_key_press), self);
    // Scrolled window might also need to forward some key events if terminal doesn't get them.
    // g_signal_connect(self->scrolled_window, "key-press-event", G_CALLBACK(on_terminal_key_press), self);

    // Setup GActionGroup for standard actions (copy, paste, etc.)
    self->action_group = g_simple_action_group_new();
    g_action_map_add_action_entries(G_ACTION_MAP(self->action_group),
                                    terminal_entries,
                                    G_N_ELEMENTS(terminal_entries),
                                    self); // User data for actions is self
    gtk_widget_insert_action_group(GTK_WIDGET(self), "terminal", G_ACTION_GROUP(self->action_group));

    gtk_widget_show_all(GTK_WIDGET(self)); // Show internal components
    gtk_widget_hide(GTK_WIDGET(self));     // But hide the whole widget initially; visibility managed by ensure_state.
}

/*** Public functions ***/

/**
 * spawn_terminal_in_widget:
 * @self: The #NemoTerminalWidget instance.
 *
 * Spawns a new shell process inside the VTE terminal widget.
 * It determines the shell to use (from $SHELL or defaults), sets the
 * working directory based on `self->current_location` (if local and exists),
 * and configures `PROMPT_COMMAND` for OSC7 terminal-to-FM synchronization
 * if enabled for local terminals.
 */
void
spawn_terminal_in_widget(NemoTerminalWidget *self)
{
    g_autofree char **env = NULL;
    g_autoptr(GError) error = NULL;
    const char *shell_executable;
    g_autofree gchar *working_directory = NULL;
    GPid child_pid; // VTE handles reaping this PID

    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    self->needs_respawn = FALSE; // Reset flag as we are attempting to spawn

    // Determine shell executable
    shell_executable = g_getenv("SHELL");
    if (shell_executable == NULL || *shell_executable == '\0')
    {
        // Fallback to common default shells
        const char *default_shells[] = {"/bin/bash", "/bin/sh", NULL};
        for (int i = 0; default_shells[i]; ++i) {
            if (g_file_test(default_shells[i], G_FILE_TEST_IS_EXECUTABLE)) {
                shell_executable = default_shells[i];
                break;
            }
        }
        if (shell_executable == NULL || *shell_executable == '\0') {
            shell_executable = "/bin/sh"; // Ultimate fallback
            g_warning("SHELL environment variable not set, and common shells not found. Defaulting to /bin/sh.");
        }
    }

    // Determine working directory (only for local, non-SSH spawns)
    // For SSH, the remote shell starts in its default (e.g., home) or handled by ssh_remote_path.
    if (!self->in_ssh_mode && self->current_location != NULL)
    {
        if (G_IS_FILE(self->current_location))
        {
            // Only use native paths for local shell's CWD
            if (g_file_is_native(self->current_location) &&
                g_file_query_exists(self->current_location, NULL))
            {
                working_directory = g_file_get_path(self->current_location);
            }
            else if (!g_file_is_native(self->current_location)) {
                // Current location is remote (e.g. sftp://), spawn local shell in home.
                g_warning("Current location is remote (%s) but attempting to spawn local shell. Using home directory.",
                          g_file_get_uri_scheme(self->current_location));
                // working_directory remains NULL, VTE will use default (usually home)
            }
            else // Native path but doesn't exist
            {
                g_autofree gchar *uri_for_warning = g_file_get_uri(self->current_location);
                g_warning("Current local location %s no longer exists. Spawning terminal in home directory.",
                          uri_for_warning ? uri_for_warning : "(unknown URI)");
                g_set_object(&self->current_location, NULL); // Reset invalid location
                // working_directory remains NULL
            }
        }
        else // self->current_location is not a GFile (should not happen if logic is correct)
        {
            g_warning("self->current_location is not a GFile in spawn_terminal_in_widget. Spawning terminal in home directory.");
            g_set_object(&self->current_location, NULL); // Reset invalid location
        }
    }

    char *argv[] = {(char *)shell_executable, NULL}; // Arguments for the shell

    // Spawn the shell process in the VTE terminal
    vte_terminal_spawn_sync(self->terminal,
                           VTE_PTY_DEFAULT,       // PTY flags
                           working_directory,     // Working directory (can be NULL for default)
                           argv,                  // Command and arguments
                           (char **)env,          // Environment variables (can be NULL for current)
                           G_SPAWN_SEARCH_PATH | G_SPAWN_CHILD_INHERITS_STDIN, // Spawn flags
                           NULL, NULL,            // Child setup function and data (unused)
                           &child_pid,            // Returns child PID (unused by us directly)
                           NULL,                  // Cancellable (unused)
                           &error);               // GError for reporting issues

    if (error != NULL)
    {
        g_warning("Failed to spawn terminal (shell: %s, wd: %s): %s",
                  shell_executable, working_directory ? working_directory : "(default)", error->message);
    }
    // env is freed by g_autofree
}

/**
 * nemo_terminal_widget_get_default_height:
 *
 * Retrieves the default/saved height for the terminal widget from GSettings.
 *
 * Returns: The terminal height in pixels. Defaults to 300 if setting is invalid or too small.
 */
int
nemo_terminal_widget_get_default_height(void)
{
    int saved_height = g_settings_get_int(nemo_window_state, "terminal-height");
    // Ensure a minimum sensible height
    return (saved_height > 50 && saved_height < 8000) ? saved_height : 300;
}

/**
 * nemo_terminal_widget_save_height:
 * @self: The #NemoTerminalWidget instance.
 * @height: The height in pixels to save.
 *
 * Saves the terminal's height. Updates the internal `self->height` and
 * persists the value to GSettings if it's within a reasonable range.
 */
void
nemo_terminal_widget_save_height(NemoTerminalWidget *self, int height)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    // Save only if height is within a reasonable range to avoid extreme values
    if (height > 50 && height < 8000) // Min 50px, Max 8000px (arbitrary upper limit)
    {
        if (self->height != height) { // Only save if changed
            self->height = height;
            g_settings_set_int(nemo_window_state, "terminal-height", height);
        }
    }
}

/**
 * nemo_terminal_widget_apply_new_size:
 * @self: The #NemoTerminalWidget instance.
 *
 * Applies the currently stored `self->height` to the #GtkPaned container
 * that holds the terminal. This adjusts the paned's divider position.
 * Should be called when the terminal is visible and the paned is realized.
 */
void
nemo_terminal_widget_apply_new_size(NemoTerminalWidget *self)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    if (!self->container_paned || !GTK_IS_PANED(self->container_paned) ||
        !gtk_widget_get_realized(GTK_WIDGET(self->container_paned)))
        return; // Paned not set, not a paned, or not realized

    int total_height = gtk_widget_get_allocated_height(GTK_WIDGET(self->container_paned));
    if (total_height > 0 && self->height > 0)
    {
        // Calculate new paned divider position.
        // Terminal is pack2 (bottom pane). Its height is `self->height`.
        // Position = total_height - terminal_height.
        int new_pos = total_height - self->height;

        // Clamp position to be valid: 0 <= new_pos <= total_height - min_terminal_height (e.g. 50)
        if (new_pos < 0) new_pos = 0;
        // Ensure terminal retains a minimum height (e.g., 50px)
        if (new_pos > total_height - 50) new_pos = total_height - 50;

        if (new_pos >= 0 && new_pos <= total_height) { // Double check validity
             gtk_paned_set_position(GTK_PANED(self->container_paned), new_pos);
        }
    }
}

/**
 * on_paned_destroy:
 * @widget: The #GtkPaned widget that is being destroyed.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for the "destroy" signal of the container paned.
 * Clears the `self->container_paned` reference in the terminal widget
 * to prevent dangling pointers if the paned is destroyed externally.
 */
static void
on_paned_destroy(GtkWidget *widget, gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);

    if (self && NEMO_IS_TERMINAL_WIDGET(self) && self->container_paned == widget)
    {
        // Paned is being destroyed, so nullify our reference to it.
        // No need to disconnect signals here, as GTK does that on destroy.
        self->container_paned = NULL;
    }
}

/**
 * nemo_terminal_widget_initialize_in_paned:
 * @self: The #NemoTerminalWidget instance.
 * @unused_view_content: (Unused) Original content widget. The `view_overlay` is used instead.
 * @view_overlay: The #GtkWidget (typically an overlay or main view area) that will
 *                become the top child of the new #GtkPaned.
 *
 * Integrates the terminal widget into the UI by creating a new #GtkPaned.
 * The @view_overlay is reparented into the top part of the paned, and
 * the terminal widget (#NemoTerminalWidget) is placed in the bottom part.
 * The new paned then replaces @view_overlay in its original parent.
 *
 * Returns: %TRUE if initialization was successful, %FALSE otherwise.
 */
gboolean
nemo_terminal_widget_initialize_in_paned(NemoTerminalWidget *self,
                                         GtkWidget *unused_view_content,
                                         GtkWidget *view_overlay)
{
    g_return_val_if_fail(NEMO_IS_TERMINAL_WIDGET(self), FALSE);

    if (!view_overlay || !gtk_widget_get_parent(view_overlay))
    {
        g_warning("Cannot add terminal: view_overlay is NULL or has no parent.");
        return FALSE;
    }

    GtkWidget *parent_container = gtk_widget_get_parent(view_overlay);
    if (!GTK_IS_CONTAINER(parent_container)) {
        g_warning("Cannot add terminal: parent of view_overlay is not a GtkContainer.");
        return FALSE;
    }

    // Create a new vertical paned
    GtkWidget *vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    self->container_paned = vpaned; // Store reference to the paned

    // Preserve packing properties if parent was a GtkBox
    gint position_in_parent = -1;
    gboolean box_expand = TRUE, box_fill = TRUE; // Defaults for GtkBox
    guint box_padding = 0;

    if (GTK_IS_BOX(parent_container)) {
        GtkBox *box_parent = GTK_BOX(parent_container);
        gtk_box_query_child_packing(box_parent, view_overlay, &box_expand, &box_fill, &box_padding, NULL);

        // Get original position of view_overlay to reinsert paned at same spot
        g_autoptr(GList) children = gtk_container_get_children(GTK_CONTAINER(parent_container));
        position_in_parent = g_list_index(children, view_overlay);
    }

    // Reparent view_overlay into the paned
    g_object_ref(view_overlay); // Increment ref before removing from old parent
    gtk_container_remove(GTK_CONTAINER(parent_container), view_overlay);

    gtk_paned_pack1(GTK_PANED(vpaned), view_overlay, TRUE, FALSE); // view_overlay in top, resize=TRUE, shrink=FALSE
    gtk_paned_pack2(GTK_PANED(vpaned), GTK_WIDGET(self), FALSE, TRUE); // terminal in bottom, resize=FALSE, shrink=TRUE
    g_object_unref(view_overlay); // Decrement ref, paned now owns it

    // Add the new paned to the original parent container
    if (GTK_IS_BOX(parent_container)) {
        gtk_box_pack_start(GTK_BOX(parent_container), vpaned, box_expand, box_fill, box_padding);
        if (position_in_parent != -1) {
            gtk_box_reorder_child(GTK_BOX(parent_container), vpaned, position_in_parent);
        }
    } else { // For other container types (e.g., GtkOverlay, GtkGrid - though grid needs attach)
        gtk_container_add(GTK_CONTAINER(parent_container), vpaned);
    }

    // Connect signals to the paned
    if (self->container_paned) {
        g_signal_connect(self->container_paned, "notify::position",
                         G_CALLBACK(on_container_size_changed), self);
        // Also connect destroy to clear our reference if paned is removed by other means
        g_signal_connect(self->container_paned, "destroy",
                         G_CALLBACK(on_paned_destroy), self);
    }

    gtk_widget_show_all(vpaned); // Show the paned and its children (terminal is initially hidden by ensure_state)

    // Apply initial size after widgets are realized (idle callback)
    g_idle_add(apply_initial_size_idle, self);
    nemo_terminal_widget_ensure_state(self); // Set initial visibility and size

    return TRUE;
}

/**
 * nemo_terminal_widget_get_visible:
 * @self: The #NemoTerminalWidget instance.
 *
 * Checks if the terminal widget is currently considered visible.
 *
 * Returns: %TRUE if the terminal is marked as visible, %FALSE otherwise.
 *          Note: This reflects the intended state; the widget itself
 *          might still be hidden if its parent is hidden.
 */
gboolean
nemo_terminal_widget_get_visible(NemoTerminalWidget *self)
{
    g_return_val_if_fail(NEMO_IS_TERMINAL_WIDGET(self), FALSE);
    return self->is_visible;
}

/**
 * nemo_terminal_widget_ensure_state:
 * @self: The #NemoTerminalWidget instance.
 *
 * Ensures the terminal's visibility and height match the saved settings.
 * This is typically called on startup or when the UI context changes.
 * If the terminal should be visible but isn't, it's shown.
 * If it should be hidden but isn't, it's hidden.
 * The saved height is applied if visible.
 */
void
nemo_terminal_widget_ensure_state(NemoTerminalWidget *self)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    gboolean should_be_visible = g_settings_get_boolean(nemo_window_state, "terminal-visible");
    self->height = nemo_terminal_widget_get_default_height(); // Load desired height

    if (should_be_visible != self->is_visible)
    {
        // Current visibility state doesn't match setting, toggle it (without saving back, as we are applying a setting)
        // `is_manual_toggle = FALSE` because this is programmatic application of state
        nemo_terminal_widget_toggle_visible_with_save(self, FALSE);
    }
    else if (should_be_visible) // Is visible and should be visible, ensure size is applied
    {
        gtk_widget_show(GTK_WIDGET(self)); // Ensure self (the GtkBox) is shown
        if (self->container_paned && GTK_IS_WIDGET(self->container_paned)) {
             if (gtk_widget_get_realized(GTK_WIDGET(self->container_paned))) {
                nemo_terminal_widget_apply_new_size(self);
             } else {
                // If not realized, schedule size application for later
                g_idle_add(apply_initial_size_idle, self);
             }
        }
        if (self->needs_respawn) { // If shell exited while hidden
            spawn_terminal_in_widget(self);
        }
    } else { // Is not visible and should not be visible
        gtk_widget_hide(GTK_WIDGET(self));
    }
}

/**
 * nemo_terminal_widget_toggle_visible_with_save:
 * @self: The #NemoTerminalWidget instance.
 * @is_manual_toggle: %TRUE if the toggle was initiated by direct user action (e.g., F4 key),
 *                    %FALSE if programmatic (e.g., applying settings).
 *
 * Toggles the visibility of the terminal widget. If becoming visible,
 * applies its saved height and may attempt to grab focus if it's a manual toggle.
 * The new visibility state is saved to GSettings.
 * Emits the "toggle-visibility" signal.
 */
void
nemo_terminal_widget_toggle_visible_with_save(NemoTerminalWidget *self,
                                              gboolean is_manual_toggle)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    if (self->in_toggling) return; // Debounce: avoid rapid toggles
    self->in_toggling = TRUE;

    self->is_visible = !self->is_visible; // Toggle the state

    if (self->is_visible)
    {
        gtk_widget_show(GTK_WIDGET(self)); // Show the terminal widget (the GtkBox)
        if (self->container_paned && GTK_IS_WIDGET(self->container_paned)) {
            // Apply size when shown
            if (gtk_widget_get_realized(GTK_WIDGET(self->container_paned))) {
                nemo_terminal_widget_apply_new_size(self);
            } else {
                g_idle_add(apply_initial_size_idle, self); // Apply after realization
            }
        }

        if (self->needs_respawn) { // If shell exited while hidden, respawn now
            spawn_terminal_in_widget(self);
        }

        if (is_manual_toggle) { // If user explicitly showed it, focus it
            nemo_terminal_widget_ensure_terminal_focus(self);
        }
    }
    else // Becoming hidden
    {
        gtk_widget_hide(GTK_WIDGET(self));
    }

    // Save the new visibility state to settings
    g_settings_set_boolean(nemo_window_state, "terminal-visible", self->is_visible);

    // Emit signal about visibility change
    g_signal_emit(self, signals[TOGGLE_VISIBILITY], 0, self->is_visible);

    // Reset toggling flag after a short delay to prevent rapid re-toggling
    g_timeout_add(100, reset_toggling_flag, self); // 100ms debounce
}

/**
 * nemo_terminal_widget_toggle_visible:
 * @self: The #NemoTerminalWidget instance.
 *
 * Convenience function to toggle terminal visibility, assuming it's a manual action.
 * Calls `nemo_terminal_widget_toggle_visible_with_save()` with `is_manual_toggle = TRUE`.
 */
void
nemo_terminal_widget_toggle_visible(NemoTerminalWidget *self)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    nemo_terminal_widget_toggle_visible_with_save(self, TRUE); // Assume manual toggle
}

/**
 * nemo_terminal_widget_ensure_terminal_focus:
 * @self: The #NemoTerminalWidget instance.
 *
 * Attempts to set keyboard focus to the VTE terminal widget.
 * Uses an idle callback to ensure this happens after UI updates.
 */
void
nemo_terminal_widget_ensure_terminal_focus(NemoTerminalWidget *self)
{
    g_idle_add((GSourceFunc)gtk_widget_grab_focus, GTK_WIDGET(self->terminal));
}

/**
 * nemo_terminal_widget_set_current_location:
 * @self: The #NemoTerminalWidget instance.
 * @location: The #GFile representing the new current location. Can be %NULL.
 *
 * Sets the terminal's current location. This may involve:
 * 1. Updating `self->current_location` and notifying property changes.
 * 2. If the new location is SFTP and auto-connect is enabled (and not already in SSH),
 *    an SSH connection might be initiated.
 * 3. If not initiating SSH, and the location is different, it calls
 *    `change_directory_in_terminal()` to `cd` in the terminal (respecting sync modes).
 * 4. If @location is %NULL and not in SSH, it might respawn the terminal in the home directory.
 */
void
nemo_terminal_widget_set_current_location(NemoTerminalWidget *self,
                                          GFile *location)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    if (location != NULL) { // location can be NULL
        g_return_if_fail(G_IS_FILE(location));
    }

    // Check if the location has logically changed (different GFile or one is NULL)
    gboolean location_logically_changed = FALSE;
    if ((self->current_location == NULL && location != NULL) ||
        (self->current_location != NULL && location == NULL) ||
        (self->current_location != NULL && location != NULL && !g_file_equal(self->current_location, location))) {
        location_logically_changed = TRUE;
    }

    // Update the internal GFile object for current_location
    gboolean object_pointer_changed = g_set_object(&self->current_location, location);

    if (object_pointer_changed) // If the GFile object pointer itself changed
    {
        g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_CURRENT_LOCATION]);
    }

    // If neither the object pointer nor the logical location changed, nothing more to do.
    if (!location_logically_changed && !object_pointer_changed) {
        return;
    }

    // Handle SSH auto-connection if navigating to an SFTP path
    if (!self->in_ssh_mode && location != NULL)
    {
        g_autofree gchar *uri = g_file_get_uri(location);
        if (uri && g_str_has_prefix(uri, "sftp://"))
        {
            if (self->ssh_auto_connect_mode != NEMO_TERMINAL_SSH_AUTOCONNECT_OFF)
            {
                g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;
                if (parse_gvfs_ssh_path(location, &hostname, &username, &port))
                {
                    NemoTerminalSyncMode sync_mode_for_auto_conn;
                    // Determine sync mode based on auto-connect setting
                    switch (self->ssh_auto_connect_mode) {
                        case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH: sync_mode_for_auto_conn = NEMO_TERMINAL_SYNC_BOTH; break;
                        case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM: sync_mode_for_auto_conn = NEMO_TERMINAL_SYNC_FM_TO_TERM; break;
                        case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM: sync_mode_for_auto_conn = NEMO_TERMINAL_SYNC_TERM_TO_FM; break;
                        case NEMO_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE: sync_mode_for_auto_conn = NEMO_TERMINAL_SYNC_NONE; break;
                        default: g_warning("Invalid SSH auto-connect mode: %d", self->ssh_auto_connect_mode); return; // Abort
                    }
                    _initiate_ssh_connection(self, hostname, username, port, sync_mode_for_auto_conn);
                    // SSH connection initiated, further 'cd' will be handled by SSH logic
                    return; // Don't fall through to change_directory_in_terminal for local
                }
                else { // Failed to parse SFTP path for auto-connect
                    g_warning("Failed to parse SFTP path for auto-connection: %s", uri);
                    // Proceed to treat as a GVFS mount path if local sync is on.
                }
            }
            else // SSH auto-connect is OFF
            {
                // If local sync FM->Term is on, and this is a GVFS sftp mount, cd to the *local mount point*.
                if (self->local_sync_mode == NEMO_TERMINAL_SYNC_BOTH ||
                    self->local_sync_mode == NEMO_TERMINAL_SYNC_FM_TO_TERM)
                {
                    g_autofree gchar *local_path = g_file_get_path(location); // Path to GVFS mount point
                    if (local_path && g_str_has_prefix(local_path, "/run/user/") && strstr(local_path, "/gvfs/sftp:host="))
                    {
                        // This is a GVFS SFTP mount path. CD to it locally.
                        // Use the original 'location' GFile which represents this local mount point.
                        change_directory_in_terminal(self, location);
                        return; // Handled
                    }
                }
                // If not syncing locally or not a GVFS path, do nothing for SFTP if auto-connect is off.
                return;
            }
        }
        else // Not an SFTP URI, must be local or other non-SSH remote
        {
            if (location_logically_changed) { // Standard local directory change
                 change_directory_in_terminal(self, location);
            }
        }
    }
    else if (self->in_ssh_mode && location != NULL) // Already in SSH mode, FM location changed
    {
        if (location_logically_changed) { // If FM navigates while in SSH, sync if enabled
            change_directory_in_terminal(self, location);
        }
    }
    else if (!location) { // Location became NULL (e.g., navigating to "Computer://")
        if (!self->in_ssh_mode && location_logically_changed) {
            // If local terminal and location becomes invalid/null, reset to home by respawning.
            spawn_terminal_in_widget(self);
        }
        // If in SSH mode and location becomes NULL, typically do nothing, keep SSH session as is.
    }
}

/**
 * nemo_terminal_widget_new_with_location:
 * @location: (Optional) The initial #GFile location for the terminal.
 *            If %NULL, the terminal will start in the default directory (e.g., home).
 *
 * Creates a new #NemoTerminalWidget. If @location is provided, it's set
 * as the initial current location. The terminal spawns a shell process.
 *
 * Returns: A new #NemoTerminalWidget instance. The caller owns the returned object.
 */
NemoTerminalWidget *
nemo_terminal_widget_new_with_location(GFile *location)
{
    // Create instance using GObject new
    NemoTerminalWidget *self = g_object_new(NEMO_TYPE_TERMINAL_WIDGET, NULL);

    if (location)
    {
        g_return_val_if_fail(G_IS_FILE(location), NULL); // Should not happen if caller is sane
        // Set initial location without triggering full sync logic yet, as spawn will handle initial CWD.
        g_set_object(&self->current_location, location);
    }

    self->height = nemo_terminal_widget_get_default_height(); // Load default height
    spawn_terminal_in_widget(self); // Spawn shell; uses self->current_location if set and local
    // Initial visibility and placement are handled by ensure_state and initialize_in_paned.

    return self;
}

/* Terminal color scheme definitions */
typedef struct
{
    GdkRGBA foreground;
    GdkRGBA background;
    GdkRGBA palette[16];        // Standard 16 ANSI colors
    gboolean use_system_colors; // If TRUE, VTE uses system theme colors
} NemoTerminalColorPalette;

// "System" theme: delegates to VTE's default behavior (often respects GTK theme)
static const NemoTerminalColorPalette system_palette = {
    .use_system_colors = TRUE
};

// A basic dark theme
static const NemoTerminalColorPalette dark_palette = {
    .foreground = {.red = 0.9,  .green = 0.9,  .blue = 0.9,  .alpha = 1.0}, // Light gray text
    .background = {.red = 0.12, .green = 0.12, .blue = 0.12, .alpha = 1.0}, // Dark gray background
    .palette = { // Standard 16 colors (8 normal, 8 bright)
        {.red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0}, /* Black */
        {.red = 0.8, .green = 0.0, .blue = 0.0, .alpha = 1.0}, /* Red */
        {.red = 0.0, .green = 0.8, .blue = 0.0, .alpha = 1.0}, /* Green */
        {.red = 0.8, .green = 0.8, .blue = 0.0, .alpha = 1.0}, /* Yellow */
        {.red = 0.0, .green = 0.0, .blue = 0.8, .alpha = 1.0}, /* Blue */
        {.red = 0.8, .green = 0.0, .blue = 0.8, .alpha = 1.0}, /* Magenta */
        {.red = 0.0, .green = 0.8, .blue = 0.8, .alpha = 1.0}, /* Cyan */
        {.red = 0.8, .green = 0.8, .blue = 0.8, .alpha = 1.0}, /* White */
        {.red = 0.5, .green = 0.5, .blue = 0.5, .alpha = 1.0}, /* Bright Black (Grey) */
        {.red = 1.0, .green = 0.4, .blue = 0.4, .alpha = 1.0}, /* Bright Red */
        {.red = 0.4, .green = 1.0, .blue = 0.4, .alpha = 1.0}, /* Bright Green */
        {.red = 1.0, .green = 1.0, .blue = 0.4, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.4, .green = 0.4, .blue = 1.0, .alpha = 1.0}, /* Bright Blue */
        {.red = 1.0, .green = 0.4, .blue = 1.0, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.4, .green = 1.0, .blue = 1.0, .alpha = 1.0}, /* Bright Cyan */
        {.red = 1.0, .green = 1.0, .blue = 1.0, .alpha = 1.0}  /* Bright White */
    },
    .use_system_colors = FALSE
};

// A basic light theme
static const NemoTerminalColorPalette light_palette = {
    .foreground = {.red = 0.15, .green = 0.15, .blue = 0.15, .alpha = 1.0}, // Dark gray text
    .background = {.red = 0.98, .green = 0.98, .blue = 0.98, .alpha = 1.0}, // Very light gray background
    .palette = {
        {.red = 0.2, .green = 0.2, .blue = 0.2, .alpha = 1.0}, /* Black */
        {.red = 0.8, .green = 0.2, .blue = 0.2, .alpha = 1.0}, /* Red */
        {.red = 0.1, .green = 0.6, .blue = 0.1, .alpha = 1.0}, /* Green */
        {.red = 0.7, .green = 0.6, .blue = 0.1, .alpha = 1.0}, /* Yellow */
        {.red = 0.2, .green = 0.4, .blue = 0.7, .alpha = 1.0}, /* Blue */
        {.red = 0.6, .green = 0.3, .blue = 0.5, .alpha = 1.0}, /* Magenta */
        {.red = 0.3, .green = 0.6, .blue = 0.7, .alpha = 1.0}, /* Cyan */
        {.red = 0.7, .green = 0.7, .blue = 0.7, .alpha = 1.0}, /* White */
        {.red = 0.4, .green = 0.4, .blue = 0.4, .alpha = 1.0}, /* Bright Black (Grey) */
        {.red = 0.9, .green = 0.3, .blue = 0.3, .alpha = 1.0}, /* Bright Red */
        {.red = 0.2, .green = 0.7, .blue = 0.2, .alpha = 1.0}, /* Bright Green */
        {.red = 0.8, .green = 0.7, .blue = 0.2, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.3, .green = 0.5, .blue = 0.8, .alpha = 1.0}, /* Bright Blue */
        {.red = 0.7, .green = 0.4, .blue = 0.6, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.4, .green = 0.7, .blue = 0.8, .alpha = 1.0}, /* Bright Cyan */
        {.red = 0.9, .green = 0.9, .blue = 0.9, .alpha = 1.0}  /* Bright White */
    },
    .use_system_colors = FALSE
};

// Solarized Dark theme
static const NemoTerminalColorPalette solarized_dark_palette = {
    .foreground = {.red = 0.8235, .green = 0.8588, .blue = 0.8706, .alpha = 1.0}, // base0
    .background = {.red = 0.0000, .green = 0.1686, .blue = 0.2118, .alpha = 1.0}, // base03
    .palette = {
        {.red = 0.0275, .green = 0.2118, .blue = 0.2588, .alpha = 1.0}, // base02
        {.red = 0.8627, .green = 0.1961, .blue = 0.1843, .alpha = 1.0}, // red
        {.red = 0.5216, .green = 0.6000, .blue = 0.0000, .alpha = 1.0}, // green
        {.red = 0.7098, .green = 0.5412, .blue = 0.0000, .alpha = 1.0}, // yellow
        {.red = 0.1490, .green = 0.5451, .blue = 0.8235, .alpha = 1.0}, // blue
        {.red = 0.8275, .green = 0.2118, .blue = 0.5098, .alpha = 1.0}, // magenta
        {.red = 0.1647, .green = 0.6314, .blue = 0.6000, .alpha = 1.0}, // cyan
        {.red = 0.9294, .green = 0.9098, .blue = 0.8353, .alpha = 1.0}, // base2
        {.red = 0.0000, .green = 0.1686, .blue = 0.2118, .alpha = 1.0}, // base03 (Bright Black)
        {.red = 0.8000, .green = 0.2588, .blue = 0.2078, .alpha = 1.0}, // orange (Bright Red)
        {.red = 0.3725, .green = 0.4235, .blue = 0.4314, .alpha = 1.0}, // base01 (Bright Green)
        {.red = 0.4078, .green = 0.4745, .blue = 0.4784, .alpha = 1.0}, // base00 (Bright Yellow)
        {.red = 0.5137, .green = 0.5804, .blue = 0.5843, .alpha = 1.0}, // base0 (Bright Blue)
        {.red = 0.4235, .green = 0.4431, .blue = 0.6118, .alpha = 1.0}, // violet (Bright Magenta)
        {.red = 0.5804, .green = 0.6078, .blue = 0.5373, .alpha = 1.0}, // base1 (Bright Cyan)
        {.red = 0.9922, .green = 0.9647, .blue = 0.8902, .alpha = 1.0}  // base3 (Bright White)
    },
    .use_system_colors = FALSE
};

// Solarized Light theme
static const NemoTerminalColorPalette solarized_light_palette = {
    .foreground = {.red = 0.4000, .green = 0.4784, .blue = 0.5098, .alpha = 1.0}, // base00
    .background = {.red = 0.9922, .green = 0.9647, .blue = 0.8902, .alpha = 1.0}, // base3
    .palette = {
        {.red = 0.0275, .green = 0.2118, .blue = 0.2588, .alpha = 1.0}, // base02
        {.red = 0.8627, .green = 0.1961, .blue = 0.1843, .alpha = 1.0}, // red
        {.red = 0.5216, .green = 0.6000, .blue = 0.0000, .alpha = 1.0}, // green
        {.red = 0.7098, .green = 0.5412, .blue = 0.0000, .alpha = 1.0}, // yellow
        {.red = 0.1490, .green = 0.5451, .blue = 0.8235, .alpha = 1.0}, // blue
        {.red = 0.8275, .green = 0.2118, .blue = 0.5098, .alpha = 1.0}, // magenta
        {.red = 0.1647, .green = 0.6314, .blue = 0.6000, .alpha = 1.0}, // cyan
        {.red = 0.9294, .green = 0.9098, .blue = 0.8353, .alpha = 1.0}, // base2
        {.red = 0.0000, .green = 0.1686, .blue = 0.2118, .alpha = 1.0}, // base03 (Bright Black)
        {.red = 0.8000, .green = 0.2588, .blue = 0.2078, .alpha = 1.0}, // orange (Bright Red)
        {.red = 0.3725, .green = 0.4235, .blue = 0.4314, .alpha = 1.0}, // base01 (Bright Green)
        {.red = 0.4078, .green = 0.4745, .blue = 0.4784, .alpha = 1.0}, // base00 (Bright Yellow)
        {.red = 0.5137, .green = 0.5804, .blue = 0.5843, .alpha = 1.0}, // base0 (Bright Blue)
        {.red = 0.4235, .green = 0.4431, .blue = 0.6118, .alpha = 1.0}, // violet (Bright Magenta)
        {.red = 0.5804, .green = 0.6078, .blue = 0.5373, .alpha = 1.0}, // base1 (Bright Cyan)
        {.red = 0.8235, .green = 0.8588, .blue = 0.8706, .alpha = 1.0}  // base0 (Bright White)
    },
    .use_system_colors = FALSE
};

// Matrix theme (green on black)
static const NemoTerminalColorPalette matrix_palette = {
    .foreground = {.red = 0.1, .green = 0.9, .blue = 0.1, .alpha = 1.0}, // Bright green text
    .background = {.red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0}, // Pure black background
    .palette = {
        {.red = 0.0, .green = 0.0, .blue = 0.0, .alpha = 1.0}, /* Black */
        {.red = 0.0, .green = 0.5, .blue = 0.0, .alpha = 1.0}, /* Red (as dark green) */
        {.red = 0.0, .green = 0.8, .blue = 0.0, .alpha = 1.0}, /* Green */
        {.red = 0.1, .green = 0.6, .blue = 0.0, .alpha = 1.0}, /* Yellow (as yellow-green) */
        {.red = 0.0, .green = 0.4, .blue = 0.0, .alpha = 1.0}, /* Blue (as darker green) */
        {.red = 0.1, .green = 0.5, .blue = 0.1, .alpha = 1.0}, /* Magenta (as mid-green) */
        {.red = 0.0, .green = 0.7, .blue = 0.1, .alpha = 1.0}, /* Cyan (as cyan-green) */
        {.red = 0.1, .green = 0.9, .blue = 0.1, .alpha = 1.0}, /* White (as bright green) */
        {.red = 0.0, .green = 0.3, .blue = 0.0, .alpha = 1.0}, /* Bright Black (very dark green) */
        {.red = 0.0, .green = 0.6, .blue = 0.0, .alpha = 1.0}, /* Bright Red */
        {.red = 0.0, .green = 1.0, .blue = 0.0, .alpha = 1.0}, /* Bright Green (full green) */
        {.red = 0.2, .green = 0.7, .blue = 0.0, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.0, .green = 0.5, .blue = 0.0, .alpha = 1.0}, /* Bright Blue */
        {.red = 0.2, .green = 0.6, .blue = 0.2, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.0, .green = 0.8, .blue = 0.2, .alpha = 1.0}, /* Bright Cyan */
        {.red = 0.2, .green = 1.0, .blue = 0.2, .alpha = 1.0}  /* Bright White (very bright green) */
    },
    .use_system_colors = FALSE
};

// One Half Dark theme (approximated from popular editor themes)
static const NemoTerminalColorPalette one_half_dark_palette = {
    .foreground = {.red = 0.870, .green = 0.870, .blue = 0.870, .alpha = 1.0}, // abb2bf
    .background = {.red = 0.157, .green = 0.168, .blue = 0.184, .alpha = 1.0}, // 282c34
    .palette = {
        {.red = 0.157, .green = 0.168, .blue = 0.184, .alpha = 1.0},  /* Black (bg) 282c34 */
        {.red = 0.882, .green = 0.490, .blue = 0.470, .alpha = 1.0}, /* Red e06c75 */
        {.red = 0.560, .green = 0.749, .blue = 0.450, .alpha = 1.0}, /* Green 98c379 */
        {.red = 0.941, .green = 0.768, .blue = 0.470, .alpha = 1.0}, /* Yellow e5c07b */
        {.red = 0.400, .green = 0.627, .blue = 0.850, .alpha = 1.0}, /* Blue 61afef */
        {.red = 0.768, .green = 0.470, .blue = 0.800, .alpha = 1.0}, /* Magenta c678dd */
        {.red = 0.341, .green = 0.709, .blue = 0.729, .alpha = 1.0}, /* Cyan 56b6c2 */
        {.red = 0.870, .green = 0.870, .blue = 0.870, .alpha = 1.0}, /* White (fg) abb2bf */
        {.red = 0.400, .green = 0.450, .blue = 0.500, .alpha = 1.0}, /* Bright Black 5c6370 (comments) */
        {.red = 0.882, .green = 0.490, .blue = 0.470, .alpha = 1.0}, /* Bright Red (same as normal) */
        {.red = 0.560, .green = 0.749, .blue = 0.450, .alpha = 1.0}, /* Bright Green */
        {.red = 0.941, .green = 0.768, .blue = 0.470, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.400, .green = 0.627, .blue = 0.850, .alpha = 1.0}, /* Bright Blue */
        {.red = 0.768, .green = 0.470, .blue = 0.800, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.341, .green = 0.709, .blue = 0.729, .alpha = 1.0}, /* Bright Cyan */
        {.red = 0.970, .green = 0.970, .blue = 0.970, .alpha = 1.0}  /* Bright White (lighter fg) */
    },
    .use_system_colors = FALSE
};

// One Half Light theme (approximated)
static const NemoTerminalColorPalette one_half_light_palette = {
    .foreground = {.red = 0.220, .green = 0.240, .blue = 0.260, .alpha = 1.0}, // 383a42 (text)
    .background = {.red = 0.980, .green = 0.980, .blue = 0.980, .alpha = 1.0}, //fafafa (bg)
    .palette = {
        {.red = 0.220, .green = 0.240, .blue = 0.260, .alpha = 1.0}, /* Black (fg) 383a42 */
        {.red = 0.858, .green = 0.200, .blue = 0.180, .alpha = 1.0}, /* Red e45649 */
        {.red = 0.310, .green = 0.600, .blue = 0.110, .alpha = 1.0}, /* Green 50a14f */
        {.red = 0.850, .green = 0.588, .blue = 0.100, .alpha = 1.0}, /* Yellow c18401 */
        {.red = 0.231, .green = 0.490, .blue = 0.749, .alpha = 1.0}, /* Blue 4078f2 */
        {.red = 0.670, .green = 0.270, .blue = 0.729, .alpha = 1.0}, /* Magenta a626a4 */
        {.red = 0.149, .green = 0.639, .blue = 0.678, .alpha = 1.0}, /* Cyan 0184bc */
        {.red = 0.800, .green = 0.800, .blue = 0.800, .alpha = 1.0}, /* White (light gray) a0a1a7 */
        {.red = 0.400, .green = 0.400, .blue = 0.400, .alpha = 1.0}, /* Bright Black (gray comments) 696c77 */
        {.red = 0.858, .green = 0.200, .blue = 0.180, .alpha = 1.0}, /* Bright Red */
        {.red = 0.310, .green = 0.600, .blue = 0.110, .alpha = 1.0}, /* Bright Green */
        {.red = 0.850, .green = 0.588, .blue = 0.100, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.231, .green = 0.490, .blue = 0.749, .alpha = 1.0}, /* Bright Blue */
        {.red = 0.670, .green = 0.270, .blue = 0.729, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.149, .green = 0.639, .blue = 0.678, .alpha = 1.0}, /* Bright Cyan */
        {.red = 0.080, .green = 0.080, .blue = 0.080, .alpha = 1.0}  /* Bright White (darkest text) 14161a */
    },
    .use_system_colors = FALSE
};

// Monokai theme (classic approximation)
static const NemoTerminalColorPalette monokai_palette = {
    .foreground = {.red = 0.929, .green = 0.925, .blue = 0.910, .alpha = 1.0}, // f8f8f2
    .background = {.red = 0.153, .green = 0.157, .blue = 0.149, .alpha = 1.0}, // 272822
    .palette = {
        {.red = 0.153, .green = 0.157, .blue = 0.149, .alpha = 1.0}, /* Black (bg) 272822 */
        {.red = 0.980, .green = 0.149, .blue = 0.450, .alpha = 1.0}, /* Red f92672 */
        {.red = 0.650, .green = 0.890, .blue = 0.180, .alpha = 1.0}, /* Green a6e22e */
        {.red = 0.960, .green = 0.780, .blue = 0.310, .alpha = 1.0}, /* Yellow f4bf75 */
        {.red = 0.208, .green = 0.580, .blue = 0.839, .alpha = 1.0}, /* Blue 66d9ef (often cyan used as blue) */
        {.red = 0.670, .green = 0.380, .blue = 0.960, .alpha = 1.0}, /* Magenta ae81ff */
        {.red = 0.239, .green = 0.909, .blue = 0.920, .alpha = 1.0}, /* Cyan (using a brighter cyan) 3 sensación */
        {.red = 0.929, .green = 0.925, .blue = 0.910, .alpha = 1.0}, /* White (fg) f8f8f2 */
        {.red = 0.400, .green = 0.400, .blue = 0.400, .alpha = 1.0}, /* Bright Black (comments) 75715e */
        {.red = 0.980, .green = 0.149, .blue = 0.450, .alpha = 1.0}, /* Bright Red */
        {.red = 0.650, .green = 0.890, .blue = 0.180, .alpha = 1.0}, /* Bright Green */
        {.red = 0.960, .green = 0.780, .blue = 0.310, .alpha = 1.0}, /* Bright Yellow */
        {.red = 0.208, .green = 0.580, .blue = 0.839, .alpha = 1.0}, /* Bright Blue */
        {.red = 0.670, .green = 0.380, .blue = 0.960, .alpha = 1.0}, /* Bright Magenta */
        {.red = 0.400, .green = 0.950, .blue = 0.950, .alpha = 1.0}, /* Bright Cyan (very bright) */
        {.red = 1.000, .green = 1.000, .blue = 1.000, .alpha = 1.0}  /* Bright White (pure white) */
    },
    .use_system_colors = FALSE
};

/**
 * nemo_terminal_widget_get_color_scheme:
 * @self: The #NemoTerminalWidget instance.
 *
 * Retrieves the name of the currently active color scheme.
 * If not already loaded from GSettings, it loads it and defaults to "system"
 * if the setting is missing or empty.
 *
 * Returns: A string representing the current color scheme name (e.g., "system", "dark").
 *          This string is owned by the widget instance or is a literal and should not be freed by the caller.
 */
const gchar *
nemo_terminal_widget_get_color_scheme(NemoTerminalWidget *self)
{
    g_return_val_if_fail(NEMO_IS_TERMINAL_WIDGET(self), "system"); // Default "system" on failure

    if (self->color_scheme == NULL) // Lazy load from settings
    {
        self->color_scheme = g_settings_get_string(nemo_window_state, "terminal-color-scheme");
        // If setting is NULL, empty, or invalid, default to "system"
        if (self->color_scheme == NULL || *self->color_scheme == '\0') {
            g_free(self->color_scheme); // Safe if NULL
            self->color_scheme = g_strdup("system"); // Ensure it's a valid, owned string
        }
        // Further validation against COLOR_SCHEME_ENTRIES could be done here if needed
    }
    return self->color_scheme;
}

/**
 * nemo_terminal_widget_set_color_scheme:
 * @self: The #NemoTerminalWidget instance.
 * @scheme_name: The name of the color scheme to set (e.g., "dark", "solarized-light").
 *
 * Sets the terminal's color scheme. If the provided @scheme_name is different
 * from the current one and is valid, it updates the internal state, saves the
 * new scheme name to GSettings, and applies the scheme to the VTE terminal.
 * If @scheme_name is invalid, it defaults to "system".
 */
void
nemo_terminal_widget_set_color_scheme(NemoTerminalWidget *self, const gchar *scheme_name)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));
    g_return_if_fail(scheme_name != NULL);

    // Validate the scheme name against known schemes
    gboolean is_valid_scheme = FALSE;
    for (gsize i = 0; i < G_N_ELEMENTS(COLOR_SCHEME_ENTRIES); ++i) {
        if (g_strcmp0(scheme_name, COLOR_SCHEME_ENTRIES[i].id) == 0) {
            is_valid_scheme = TRUE;
            break;
        }
    }

    if (!is_valid_scheme) {
        g_warning("Invalid terminal color scheme requested: '%s'. Defaulting to 'system'.", scheme_name);
        scheme_name = "system"; // Fallback to a known default
    }

    // Only update if the scheme has actually changed
    if (g_strcmp0(nemo_terminal_widget_get_color_scheme(self), scheme_name) != 0) {
        g_free(self->color_scheme); // Free old scheme name string
        self->color_scheme = g_strdup(scheme_name); // Store new one

        g_settings_set_string(nemo_window_state, "terminal-color-scheme", self->color_scheme);
        nemo_terminal_widget_apply_color_scheme(self); // Apply the new scheme visually
    }
}

/**
 * nemo_terminal_widget_apply_color_scheme:
 * @self: The #NemoTerminalWidget instance.
 *
 * Applies the currently selected color scheme (stored in `self->color_scheme`)
 * to the VTE terminal widget. This involves setting foreground, background,
 * and palette colors, or resetting to system colors if "system" scheme is chosen.
 */
void
nemo_terminal_widget_apply_color_scheme(NemoTerminalWidget *self)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    const NemoTerminalColorPalette *palette_to_apply = NULL;
    const gchar *current_scheme_name = nemo_terminal_widget_get_color_scheme(self);

    // Map scheme name to its corresponding palette definition
    if (g_strcmp0(current_scheme_name, "dark") == 0)
        palette_to_apply = &dark_palette;
    else if (g_strcmp0(current_scheme_name, "light") == 0)
        palette_to_apply = &light_palette;
    else if (g_strcmp0(current_scheme_name, "solarized-dark") == 0)
        palette_to_apply = &solarized_dark_palette;
    else if (g_strcmp0(current_scheme_name, "solarized-light") == 0)
        palette_to_apply = &solarized_light_palette;
    else if (g_strcmp0(current_scheme_name, "matrix") == 0)
        palette_to_apply = &matrix_palette;
    else if (g_strcmp0(current_scheme_name, "one-half-dark") == 0)
        palette_to_apply = &one_half_dark_palette;
    else if (g_strcmp0(current_scheme_name, "one-half-light") == 0)
        palette_to_apply = &one_half_light_palette;
    else if (g_strcmp0(current_scheme_name, "monokai") == 0)
        palette_to_apply = &monokai_palette;
    else // Default to "system" scheme (includes explicit "system" or unrecognized)
        palette_to_apply = &system_palette;

    // Apply the chosen palette to the VTE terminal
    if (palette_to_apply->use_system_colors)
    {
        // Reset to VTE/system default colors
        // Passing NULL or a zeroed GdkRGBA typically resets to defaults.
        GdkRGBA default_color = {0}; // Zeroed structure
        vte_terminal_set_color_background(self->terminal, &default_color); // Reset background
        vte_terminal_set_color_foreground(self->terminal, &default_color); // Reset foreground
        vte_terminal_set_colors(self->terminal, NULL, NULL, NULL, 0); // Reset palette
    }
    else
    {
        // Apply the custom foreground, background, and 16-color palette
        vte_terminal_set_colors(self->terminal,
                                &palette_to_apply->foreground,
                                &palette_to_apply->background,
                                palette_to_apply->palette, // Array of GdkRGBA
                                G_N_ELEMENTS(palette_to_apply->palette)); // Count of palette colors
    }
}

/**
 * nemo_terminal_widget_finalize:
 * @object: The #NemoTerminalWidget GObject instance being finalized.
 *
 * GObject finalize function. Frees allocated resources associated with the
 * widget instance, such as GFile objects, action groups, strings, and
 * disconnects signals from external objects if necessary.
 */
static void
nemo_terminal_widget_finalize(GObject *object)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(object);

    // Disconnect signals connected to self->container_paned if it still exists
    // This prevents callbacks on a partially destroyed 'self' if paned outlives 'self'.
    // Note: GTK usually handles disconnection from destroyed objects, but explicit is safer for non-child objects.
    if (self->container_paned && GTK_IS_WIDGET(self->container_paned))
    {
        g_signal_handlers_disconnect_by_func(self->container_paned, G_CALLBACK(on_container_size_changed), self);
        g_signal_handlers_disconnect_by_func(self->container_paned, G_CALLBACK(on_paned_destroy), self);
        // Do not unref container_paned here, it's owned by its parent GTK container.
        // on_paned_destroy should set self->container_paned to NULL if it's destroyed first.
    }
    self->container_paned = NULL; // Clear reference

    // Clean up GObject resources
    g_clear_object(&self->current_location);
    g_clear_object(&self->action_group);

    // Free allocated strings
    g_free(self->color_scheme);
    self->color_scheme = NULL;

    // Clear any remaining SSH state (important for freeing SSH-related strings)
    clear_ssh_state(self);

    // Cancel any pending timeouts
    if (self->focus_timeout_id > 0) {
        g_source_remove(self->focus_timeout_id);
        self->focus_timeout_id = 0;
    }
    // (reset_toggling_flag timeout should also be handled if it were stored with an ID)

    // Chain up to the parent class's finalize method
    G_OBJECT_CLASS(nemo_terminal_widget_parent_class)->finalize(object);
}

/**
 * nemo_terminal_get_font_size:
 *
 * Retrieves the saved terminal font size (in points) from GSettings.
 *
 * Returns: The font size in points. Defaults to 12 if the setting is
 *          invalid or outside a reasonable range (6-72pt).
 */
static int
nemo_terminal_get_font_size(void)
{
    int saved_size_pts = g_settings_get_int(nemo_window_state, "terminal-font-size");
    // Validate saved size, provide a default if out of range
    return (saved_size_pts >= 6 && saved_size_pts <= 72) ? saved_size_pts : 12; // Default 12pt
}

/**
 * nemo_terminal_widget_save_font_size:
 * @self: The #NemoTerminalWidget instance.
 * @font_size_pts: The font size in points to save.
 *
 * Saves the terminal's font size (in points) to GSettings, if it's within
 * a reasonable range (6-72pt).
 */
static void
nemo_terminal_widget_save_font_size(NemoTerminalWidget *self, int font_size_pts)
{
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    // Persist only if font size is within a sensible range
    if (font_size_pts >= 6 && font_size_pts <= 72)
    {
        // Only write to GSettings if it's different from current setting to avoid unnecessary writes.
        if (g_settings_get_int(nemo_window_state, "terminal-font-size") != font_size_pts) {
            g_settings_set_int(nemo_window_state, "terminal-font-size", font_size_pts);
        }
    }
}

/**
 * build_ssh_command_string:
 * @hostname: The hostname for the SSH connection (mandatory).
 * @username: (Optional) The username for SSH.
 * @port: (Optional) The port number for SSH as a string.
 *
 * Constructs the basic SSH command line string (e.g., "ssh user@host -p 2222\n").
 * Username and hostname are shell-quoted. Port is validated to be numeric
 * and within the valid port range. The command always ends with a newline
 * character, suitable for direct feeding to `vte_terminal_feed_child` to execute.
 *
 * Returns: A newly allocated string containing the SSH command.
 *          The caller must free this string. Returns %NULL on failure (e.g. no hostname).
 */
static gchar *
build_ssh_command_string(const gchar *hostname, const gchar *username, const gchar *port)
{
    g_return_val_if_fail(hostname != NULL && *hostname != '\0', NULL);

    // GString struct itself is managed by the g_string_free call at the end when stealing the buffer.
    // Do NOT use g_autofree on cmd_builder here, as g_string_free(..., FALSE) frees the struct.
    GString *cmd_builder = g_string_new("ssh ");

    // Append username if provided
    if (username != NULL && *username != '\0')
    {
        // g_shell_quote returns a new string that must be freed. g_autofree handles this.
        g_autofree gchar *quoted_username = g_shell_quote(username);
        g_string_append_printf(cmd_builder, "%s@", quoted_username);
    }

    // Append hostname (mandatory)
    g_autofree gchar *quoted_hostname = g_shell_quote(hostname);
    g_string_append(cmd_builder, quoted_hostname);

    // Append port if provided and valid
    if (port != NULL && *port != '\0')
    {
        gboolean is_numeric_port = TRUE;
        for (const gchar *p_char = port; *p_char; ++p_char) {
            if (!g_ascii_isdigit(*p_char)) {
                is_numeric_port = FALSE;
                break;
            }
        }

        if (is_numeric_port) {
             long port_num_long = g_ascii_strtoll(port, NULL, 10); // Base 10
             if (port_num_long > 0 && port_num_long <= 65535) { // Valid TCP/UDP port range
                // Port is numeric and in range, append it. No need to quote numeric port.
                g_string_append_printf(cmd_builder, " -p %s", port);
             } else {
                g_warning("Invalid port number specified: %s. Port option will be omitted.", port);
             }
        } else {
            g_warning("Non-numeric port specified: %s. Port option will be omitted.", port);
        }
    }

    g_string_append_c(cmd_builder, '\n'); // Add newline to execute command when fed

    // Frees the GString struct cmd_builder itself, and returns ownership of the internal char* buffer.
    return g_string_free(cmd_builder, FALSE);
}


/**
 * parse_gvfs_ssh_path:
 * @location: The #GFile representing a location, potentially SFTP.
 * @hostname: (Output) Pointer to store the extracted hostname.
 * @username: (Output) Pointer to store the extracted username.
 * @port: (Output) Pointer to store the extracted port string.
 *
 * Parses a #GFile's URI or path to extract SSH connection details (hostname,
 * username, port) if it represents an SFTP location.
 * Handles "sftp://" URIs directly.
 * Also attempts to parse GVFS-style local mount paths for SFTP shares
 * (e.g., "/run/user/UID/gvfs/sftp:host=example.com,user=me/path").
 * Output parameters are allocated by this function and must be freed by the caller
 * if the function returns %TRUE. If %FALSE, their state is undefined but typically NULL.
 *
 * Returns: %TRUE if SSH details (at least hostname) were successfully parsed, %FALSE otherwise.
 */
static gboolean
parse_gvfs_ssh_path(GFile *location, gchar **hostname, gchar **username, gchar **port)
{
    g_return_val_if_fail(G_IS_FILE(location), FALSE);
    g_return_val_if_fail(hostname != NULL && username != NULL && port != NULL, FALSE);

    // Initialize output parameters to NULL
    *hostname = NULL;
    *username = NULL;
    *port = NULL;

    g_autofree gchar *uri_str = g_file_get_uri(location);
    if (uri_str == NULL) return FALSE; // Cannot proceed without a URI

    gboolean success = FALSE;

    // Try parsing as a standard "sftp://" URI first
    if (g_str_has_prefix(uri_str, "sftp://"))
    {
        g_autoptr(GUri) parsed_sftp_uri = g_uri_parse(uri_str, G_URI_FLAGS_NONE, NULL);
        if (parsed_sftp_uri) {
            const char *parsed_host_const = g_uri_get_host(parsed_sftp_uri);
            if (parsed_host_const && *parsed_host_const != '\0') {
                *hostname = g_strdup(parsed_host_const);
                success = TRUE; // At least hostname is found

                // Get user info (can be "user" or "user:password")
                // For SFTP, typically just "user". g_uri_get_user() is better if available (GLib >= 2.66)
                const char *user_info_const = g_uri_get_userinfo(parsed_sftp_uri);
                if (user_info_const && *user_info_const != '\0') {
                    // Assuming no password in userinfo for SFTP URIs from GVFS.
                    // If password could be present, strchr for ':' would be needed.
                    *username = g_strdup(user_info_const);
                }

                int port_num_int = g_uri_get_port(parsed_sftp_uri);
                // Only store port if it's non-standard (not 22) and valid.
                if (port_num_int > 0 && port_num_int <= 65535 && port_num_int != 22) {
                    *port = g_strdup_printf("%d", port_num_int);
                }
            }
        }
    }
    else // Fallback: Try parsing as a local GVFS mount path for SFTP
    {
        g_autofree gchar *local_fs_path = g_file_get_path(location);
        if (local_fs_path)
        {
            // Example path: /run/user/1000/gvfs/sftp:host=example.com,user=testuser/remote/folder
            // Look for the characteristic GVFS sftp mount string part.
            const char *gvfs_sftp_marker_prefix = "/gvfs/sftp:host="; // A common pattern
            char *sftp_details_start = strstr(local_fs_path, gvfs_sftp_marker_prefix);

            if (sftp_details_start) {
                // Move past "/gvfs/" to the start of "sftp:host=..." or "host=..."
                sftp_details_start += strlen("/gvfs/");

                // The details (host, user, port) are comma-separated before the actual remote path part.
                // Find end of connection details part (start of actual path, or end of string)
                char *path_component_start = strchr(sftp_details_start, '/');
                g_autofree gchar *details_substring = NULL;
                if (path_component_start) {
                    details_substring = g_strndup(sftp_details_start, path_component_start - sftp_details_start);
                } else {
                    details_substring = g_strdup(sftp_details_start);
                }

                g_auto(GStrv) parts = g_strsplit(details_substring, ",", -1);
                for (gchar **part_iter = parts; part_iter && *part_iter; ++part_iter)
                {
                    if (g_str_has_prefix(*part_iter, "sftp:host=")) {
                        g_free(*hostname); // Free previous if any (e.g. from "host=")
                        *hostname = g_strdup(*part_iter + strlen("sftp:host="));
                    } else if (g_str_has_prefix(*part_iter, "host=") && *hostname == NULL) { // Only if sftp:host not found
                        *hostname = g_strdup(*part_iter + strlen("host="));
                    } else if (g_str_has_prefix(*part_iter, "user=")) {
                        g_free(*username);
                        *username = g_strdup(*part_iter + strlen("user="));
                    } else if (g_str_has_prefix(*part_iter, "port=")) {
                        g_free(*port);
                        *port = g_strdup(*part_iter + strlen("port="));
                    }
                }
                // Success if hostname was found
                if (*hostname != NULL && **hostname != '\0') {
                    success = TRUE;
                }
            }
        }
    }

    // If parsing failed but memory was allocated for outputs, free it.
    if (!success) {
        g_clear_pointer(hostname, g_free);
        g_clear_pointer(username, g_free);
        g_clear_pointer(port, g_free);
    }
    return success;
}

/**
 * on_terminal_contents_changed:
 * @terminal: The #VteTerminal whose contents changed.
 * @user_data: The #NemoTerminalWidget instance.
 *
 * Callback for VTE's "contents-changed" signal.
 * This is used heuristically to detect when an SSH connection has likely
 * become "live" (i.e., a shell prompt or login message appears).
 * When `self->ssh_connecting` is TRUE, it scans recent terminal output
 * for common prompt indicators. If found, it finalizes the SSH setup:
 * - Sets up PROMPT_COMMAND for Term->FM sync if enabled.
 * - `cd`s to the `ssh_remote_path` if set and FM->Term sync is enabled.
 * - Grabs focus for the terminal.
 */
static void
on_terminal_contents_changed(VteTerminal *terminal,
                             gpointer user_data)
{
    NemoTerminalWidget *self = NEMO_TERMINAL_WIDGET(user_data);
    g_return_if_fail(NEMO_IS_TERMINAL_WIDGET(self));

    // If we are in the process of establishing an SSH connection:
    if (self->ssh_connecting)
    {
        // Heuristic: Check if a prompt has appeared, indicating connection established.
        // Avoid checking if there's a selection, as that might be user activity.
        if (vte_terminal_get_has_selection(terminal)) return;

        glong cursor_row, cursor_col;
        vte_terminal_get_cursor_position(terminal, &cursor_col, &cursor_row);

        if (cursor_row < 0 || cursor_col < 0) return; // Cursor position not valid

        // Check a few lines of recent output for prompt-like strings.
        // This is a heuristic and might not be 100% reliable for all SSH servers/shells.
        glong start_scan_row = MAX(0, cursor_row - 5); // Scan last 5 lines approx.
        glong terminal_cols = vte_terminal_get_column_count(terminal);

        // Get text from a range. VTE might return less if at start/end of buffer.
        g_autofree gchar *recent_text = vte_terminal_get_text_range(terminal,
                                                          start_scan_row, 0,         // Start row, col
                                                          cursor_row, terminal_cols, // End row, col
                                                          NULL, NULL, NULL);         // Predicates unused

        if (recent_text)
        {
            // Common shell prompt indicators or SSH welcome messages
            const char *prompt_indicators[] = {
                "$ ", "# ", "% ", "> ",      // Common shell prompts
                "@",                         // Often part of user@host
                "~]$", "~]#",                // Common Bash/Zsh full prompts
                "Last login:", "Welcome to", // SSH login messages
                NULL // Terminator
            };

            gboolean prompt_likely_found = FALSE;
            for (int i = 0; prompt_indicators[i]; ++i) {
                if (strstr(recent_text, prompt_indicators[i])) {
                    prompt_likely_found = TRUE;
                    break;
                }
            }

            if (prompt_likely_found)
            {
                // SSH connection seems to be live
                self->ssh_connecting = FALSE; // No longer in "connecting" state

                // If sync Term->FM is enabled for this SSH session, set up PROMPT_COMMAND on remote.
                // This is a best-effort attempt; remote shell must support PROMPT_COMMAND (e.g., bash, zsh).
                if (self->pending_ssh_sync_mode == NEMO_TERMINAL_SYNC_BOTH ||
                    self->pending_ssh_sync_mode == NEMO_TERMINAL_SYNC_TERM_TO_FM)
                {
                    // Simple PROMPT_COMMAND for OSC7.
                    const char *osc7_export_cmd = "export PROMPT_COMMAND='echo -en \"\\033]7;file://$PWD\\007\"'\n";
                    vte_terminal_feed_child(self->terminal, osc7_export_cmd, -1);
                }

                // If a remote path was stored and FM->Term sync is enabled, cd to it.
                if (self->ssh_remote_path && *self->ssh_remote_path &&
                    (self->pending_ssh_sync_mode == NEMO_TERMINAL_SYNC_BOTH ||
                     self->pending_ssh_sync_mode == NEMO_TERMINAL_SYNC_FM_TO_TERM))
                {
                    self->ignore_next_terminal_cd_signal = TRUE; // We are initiating this cd
                    feed_cd_command(self->terminal, self->ssh_remote_path);
                }

                // Connection established and initial commands sent, grab focus.
                gtk_widget_grab_focus(GTK_WIDGET(self->terminal));
            }
        }
    }
}

/**
 * feed_cd_command:
 * @terminal: The #VteTerminal to feed the command to.
 * @path: The directory path to change to.
 *
 * Feeds a "cd /path/to/directory\r" command to the terminal.
 * It attempts to preserve any text already typed by the user on the current
 * command line by using shell control sequences (Ctrl+A, Ctrl+K, Ctrl+Y).
 * This is a common technique to avoid disrupting user input, especially
 * with shells that have auto-suggestion features (like fish, zsh with plugins).
 * The path is shell-quoted.
 */
static void
feed_cd_command(VteTerminal *terminal, const char *path)
{
    g_return_if_fail(VTE_IS_TERMINAL(terminal));
    g_return_if_fail(path != NULL);

    g_autofree gchar *quoted_path = g_shell_quote(path);
    // Use \r (carriage return) to execute, some shells might prefer \n. \r is common.
    g_autofree gchar *cd_command_str = g_strdup_printf("cd %s\r", quoted_path);

    if (!cd_command_str) {
        g_warning("feed_cd_command: Failed to create cd command string for path: %s", path);
        return;
    }

    // This sequence aims to preserve user's current input line:
    // 1. \x01 (Ctrl+A): Move cursor to start of line.
    // 2. " ": Insert a space. (Ensures Ctrl+K has something to cut if line was empty, and simplifies restoration).
    // 3. \x01 (Ctrl+A): Move cursor to start of line again (before the space).
    // 4. \x0B (Ctrl+K): Kill (cut) text from cursor to end of line. This saves it to the shell's kill-ring.
    // 5. (feed cd command): Execute the `cd` command.
    // 6. \x19 (Ctrl+Y): Yank (paste) the killed text back.
    // 7. \x01 (Ctrl+A): Move cursor to start of line.
    // 8. \033[3~ (Delete): Delete the leading space that was inserted. (Standard VT100/xterm delete char sequence)
    // 9. \x05 (Ctrl+E): Move cursor to end of line. (Restores cursor position if user was typing at end)

    vte_terminal_feed_child(terminal, "\x01 ", -1);        // Ctrl+A, space
    vte_terminal_feed_child(terminal, "\x01", -1);         // Ctrl+A
    vte_terminal_feed_child(terminal, "\x0B", -1);         // Ctrl+K (cut line)
    vte_terminal_feed_child(terminal, cd_command_str, -1); // Feed "cd /new/path\r"
    vte_terminal_feed_child(terminal, "\x19", -1);         // Ctrl+Y (paste old line)
    vte_terminal_feed_child(terminal, "\x01", -1);         // Ctrl+A
    vte_terminal_feed_child(terminal, "\033[3~", -1);      // Delete char (the space)
    vte_terminal_feed_child(terminal, "\x05", -1);         // Ctrl+E (end of line)
}