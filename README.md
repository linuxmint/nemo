![build](https://github.com/linuxmint/nemo/actions/workflows/build.yml/badge.svg)

Nemo
====
Nemo is a free and open-source software and official file manager of the Cinnamon desktop environment. 
It is a fork of GNOME Files (formerly named Nautilus).

Nemo also manages the Cinnamon desktop.
Since Cinnamon 6.0 (Mint 21.3), users can enhance their own Nemo with Spices named Actions.


History
====
Nemo started as a fork of the GNOME file manager Nautilus v3.4. Version 1.0.0 was released in July 2012 along with version 1.6 of Cinnamon,
reaching version 1.1.2 in November 2012.

Developer Gwendal Le Bihan named the project "nemo" after Jules Verne's famous character Captain Nemo, who is the captain of the Nautilus.

Build Instructions
====
## 1.Install needed dependencies:
``` 
sudo apt install git meson ninja-build pkg-config build-essential \
libglib2.0-dev libgtk-3-dev libxml2-utils libgnome-desktop-3-dev \
libcinnamon-desktop-dev libx11-dev libxext-dev libnotify-dev \
libexif-dev libexempi-dev libgirepository1.0-dev libgsf-1-dev libgail-common \
libxapp-dev libgail-common libjson-glib-dev gobject-introspection 
```
## 2. Clone repo:

``` git clone https://github.com/linuxmint/nemo ```

## 3. Build:
```
mkdir build
meson setup ./build
meson compile -C ./build
```
Run with ``` ./build/src/nemo ```

Features
====
Nemo v1.0.0 had the following features as described by the developers:
1. Ability to SSH into remote servers
2. Native support for FTP (File Transfer Protocol) and MTP (Media Transfer Protocol)
3. All the features Nautilus 3.4 had and which are missing in Nautilus 3.6 (all desktop icons, compact view, etc.)
4. Open in terminal (integral part of Nemo)
5. Open as root (integral part of Nemo)
6. Uses GVfs and GIO
7. File operations progress information (when copying or moving files, one can see the percentage and information about the operation on the window title and so also in the window list)
8. Proper GTK bookmarks management
9. Full navigation options (back, forward, up, refresh)
10. Ability to toggle between the path entry and the path breadcrumb widgets
11. Many more configuration options
