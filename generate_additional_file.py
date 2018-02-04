#!/usr/bin/python3

import os

os.chdir("data")
os.system("./merge_action_strings")
os.chdir("..")

DOMAIN = "nemo"
PATH = "/usr/share/locale"

import os, gettext, sys
sys.path.append('/usr/lib/linuxmint/common')
import additionalfiles

os.environ['LANGUAGE'] = "en_US.UTF-8"
gettext.install(DOMAIN, PATH)

prefix = """[Desktop Entry]
"""

suffix = """Exec=nemo %U
Icon=folder
Terminal=false
Type=Application
StartupNotify=false
Categories=GNOME;GTK;Utility;Core;
MimeType=inode/directory;application/x-gnome-saved-search;
Actions=open-home;open-computer;open-trash;
"""

additionalfiles.generate(DOMAIN, PATH, "data/nemo.desktop.in.in", prefix, _("Files"), _("Access and organize files"), suffix)


prefix = """
[Desktop Action open-home]
"""
suffix = """Exec=nemo %U
"""
additionalfiles.generate(DOMAIN, PATH, "data/nemo.desktop.in.in", prefix, _("Home"), None, suffix, append=True)

prefix = """
[Desktop Action open-computer]
"""
suffix = """Exec=nemo computer:///
"""
additionalfiles.generate(DOMAIN, PATH, "data/nemo.desktop.in.in", prefix, _("Computer"), None, suffix, append=True)

prefix = """
[Desktop Action open-trash]
"""
suffix = """Exec=nemo trash:///
"""
additionalfiles.generate(DOMAIN, PATH, "data/nemo.desktop.in.in", prefix, _("Trash"), None, suffix, append=True)
