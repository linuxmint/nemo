# Nemo
Nemo is the file manager for the Cinnamon desktop environment. It started as a fork of Nautilus 3.4 (https://live.gnome.org/Nautilus). A list of contributors can be found on the GitHub page (https://github.com/linuxmint/nemo/graphs/contributors).

## Copying
Nemo is licensed under the GNU General Public License (version 2), the docs are licensed under the GNU Free Documentation License, and the libs are licensed under the GNU Library General Public License. All these licenses can be found in the copying directory.

Nemo extensions link against the libnemo-extenstions library which is under the LGPL license. However, they also get loaded into the main nemo program which is licensed under the GPL. So, extensions should not be incompatible with the LGPL or GPL.

Some extensions are GPL but use some IPC mechanism like dbus to talk to a potentially non-GPL application. This is actually not such a bad design in general if your extension is doing a lot of work, as running as a nemo extension with all its issues (no synchronous i/o, can't control of the context your code runs in, etc) can be kind of a pain.
