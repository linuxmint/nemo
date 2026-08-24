#!/bin/bash
# smplOS Keybinding Profile for Nemo
# Double Commander-inspired keybindings
#
# Usage:
#   GSETTINGS_SCHEMA_DIR=libnemo-private/ ./support/smplos-keybindings.sh
#
# To reset all back to defaults:
#   GSETTINGS_SCHEMA_DIR=libnemo-private/ ./support/smplos-keybindings-reset.sh

SCHEMA="org.nemo.keybindings"

echo "Applying smplOS keybinding profile..."

# --- Double Commander style ---
# F5 = Copy selection to other pane (clear reload-alt which also uses F5)
gsettings set $SCHEMA reload-alt ""
gsettings set $SCHEMA copy-to-other-pane "F5"

# F6 = Move selection to other pane (clear switch-pane which also uses F6)
gsettings set $SCHEMA switch-pane ""
gsettings set $SCHEMA move-to-other-pane "F6"

# F7 = New Folder (was: Ctrl+Shift+N)
gsettings set $SCHEMA new-folder "F7"

# F8 = Move to Trash (was: Delete)
gsettings set $SCHEMA trash "F8"

# Alt+F7 = Search (was: Ctrl+F)
gsettings set $SCHEMA search "<Alt>F7"

# --- Sidebar / Bookmarks ---
# Ctrl+B = Toggle Sidebar (was: F9) — like VS Code
gsettings set $SCHEMA show-sidebar "<Control>b"

# Ctrl+Shift+O = Edit Bookmarks (clear open-alternate which also uses Ctrl+Shift+O)
gsettings set $SCHEMA open-alternate ""
gsettings set $SCHEMA edit-bookmarks "<Control><Shift>o"

# Ctrl+D = Add Bookmark (already default, but set explicitly)
gsettings set $SCHEMA add-bookmark "<Control>d"

# --- Bookmark/Disk Picker (Double Commander Alt+F1/F2) ---
gsettings set $SCHEMA bookmark-picker "<Alt>F1"
gsettings set $SCHEMA bookmark-picker-other "<Alt>F2"

# --- Enable trash confirmation dialog ---
gsettings set org.nemo.preferences confirm-move-to-trash true

echo ""
echo "smplOS keybindings applied:"
echo "  F5           → Copy to other pane"
echo "  F6           → Move to other pane"
echo "  F7           → New folder"
echo "  F8           → Move to Trash"
echo "  Alt+F7       → Search"
echo "  Ctrl+B       → Toggle sidebar (with focus)"
echo "  Ctrl+Shift+O → Edit bookmarks"
echo "  Ctrl+D       → Add bookmark"
echo "  Alt+F1       → Bookmark/disk picker (current pane)"
echo "  Alt+F2       → Bookmark/disk picker (other pane)"
echo ""
echo "Cleared conflicts:"
echo "  reload-alt      (was F5, use Ctrl+R instead)"
echo "  switch-pane     (was F6, disabled)"
echo "  open-alternate  (was Ctrl+Shift+O, disabled)"
echo ""
echo "Restart Nemo for all changes to take effect."
