#!/bin/bash
# Reset all keybindings back to Nemo defaults
#
# Usage:
#   GSETTINGS_SCHEMA_DIR=libnemo-private/ ./support/smplos-keybindings-reset.sh

SCHEMA="org.nemo.keybindings"

echo "Resetting all keybindings to defaults..."

gsettings reset $SCHEMA copy-to-other-pane
gsettings reset $SCHEMA move-to-other-pane
gsettings reset $SCHEMA switch-pane
gsettings reset $SCHEMA new-folder
gsettings reset $SCHEMA trash
gsettings reset $SCHEMA search
gsettings reset $SCHEMA show-sidebar
gsettings reset $SCHEMA edit-bookmarks
gsettings reset $SCHEMA add-bookmark
gsettings reset $SCHEMA split-view
gsettings reset $SCHEMA reload-alt
gsettings reset $SCHEMA open-alternate
gsettings reset $SCHEMA bookmark-picker
gsettings reset $SCHEMA bookmark-picker-other

echo "Done. Restart Nemo for changes to take effect."
