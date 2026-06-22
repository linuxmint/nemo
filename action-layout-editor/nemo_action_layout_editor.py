#!/usr/bin/python3
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Nemo', '3.0')
from gi.repository import Gtk, Gdk, GLib, Gio, Nemo
import subprocess
import gettext
import locale
import os

import leconfig

locale.bindtextdomain("nemo", leconfig.LOCALE_DIR)
gettext.bindtextdomain("nemo", leconfig.LOCALE_DIR)
gettext.textdomain("nemo")
_ = gettext.gettext

USER_ACTIONS_DIR = os.path.join(GLib.get_user_data_dir(), "nemo", "actions")

class EditorWindow():
    def __init__(self):
        self.main_window = Gtk.Window()
        self.main_window.set_default_size(800, 600)
        self.main_window.set_icon_name("nemo")

        header = Gtk.HeaderBar()
        header.set_show_close_button(True)
        header.set_title(_("Nemo Actions Layout Editor"))
        self.main_window.set_titlebar(header)

        self.hamburger_button = Gtk.MenuButton(
            image=Gtk.Image.new_from_icon_name("xsi-open-menu-symbolic", Gtk.IconSize.BUTTON)
        )
        header.pack_start(self.hamburger_button)

        menu = Gtk.Menu()
        item = Gtk.ImageMenuItem(
            label=_("Open user actions folder"),
            image=Gtk.Image(icon_name="xsi-folder-symbolic", icon_size=Gtk.IconSize.MENU)
        )
        item.connect("activate", self.open_actions_folder_clicked)
        menu.add(item)

        item = Gtk.SeparatorMenuItem()
        menu.add(item)

        item = Gtk.MenuItem(label=_("Quit"))
        item.connect("activate", self.quit)
        menu.add(item)

        menu.show_all()
        self.hamburger_button.set_popup(menu)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, border_width=8)
        self.main_window.add(vbox)

        self.editor = Nemo.ActionLayoutEditor.new()
        vbox.pack_start(self.editor, True, True, 0)

        self.main_window.connect("delete-event", self.window_delete)

        self.main_window.show_all()
        self.main_window.present_with_time(0)

    def quit(self, button):
        Gtk.main_quit()
        return True

    def window_delete(self, window, event, data=None):
        Gtk.main_quit()
        return False

    def open_actions_folder_clicked(self, button):
        # Create actions directory if it doesn't exist
        os.makedirs(USER_ACTIONS_DIR, exist_ok=True)
        subprocess.Popen(["xdg-open", USER_ACTIONS_DIR])

if __name__ == "__main__":
    import sys

    EditorWindow()
    Gtk.main()

    sys.exit(0)
