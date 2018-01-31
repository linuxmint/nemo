
dbusBuiltSources = gnome.gdbus_codegen('nemo-generated',
  join_paths(meson.source_root(), 'data', 'dbus-interfaces.xml'),
  interface_prefix: 'org.Nemo.',
  namespace: 'NemoDBus',
  object_manager: true
)

nemo_private_sources = [
  dbusBuiltSources,
  'nemo-action-manager.c',
  'nemo-action.c',
  'nemo-bookmark.c',
  'nemo-cell-renderer-disk.c',
  'nemo-centered-placement-grid.c',
  'nemo-clipboard-monitor.c',
  'nemo-clipboard.c',
  'nemo-column-chooser.c',
  'nemo-column-utilities.c',
  'nemo-context-menu-menu-item.c',
  'nemo-dbus-manager.c',
  'nemo-debug.c',
  'nemo-default-file-icon.c',
  'nemo-desktop-directory-file.c',
  'nemo-desktop-directory.c',
  'nemo-desktop-icon-file.c',
  'nemo-desktop-link-monitor.c',
  'nemo-desktop-link.c',
  'nemo-desktop-metadata.c',
  'nemo-desktop-utils.c',
  'nemo-directory-async.c',
  'nemo-directory.c',
  'nemo-dnd.c',
  'nemo-entry.c',
  'nemo-file-changes-queue.c',
  'nemo-file-conflict-dialog.c',
  'nemo-file-dnd.c',
  'nemo-file-operations.c',
  'nemo-file-queue.c',
  'nemo-file-undo-manager.c',
  'nemo-file-undo-operations.c',
  'nemo-file-utilities.c',
  'nemo-file.c',
  'nemo-global-preferences.c',
  'nemo-icon-canvas-item.c',
  'nemo-icon-container.c',
  'nemo-icon-dnd.c',
  'nemo-icon-info.c',
  'nemo-job-queue.c',
  'nemo-lib-self-check-functions.c',
  'nemo-link.c',
  'nemo-merged-directory.c',
  'nemo-metadata.c',
  'nemo-mime-application-chooser.c',
  'nemo-module.c',
  'nemo-monitor.c',
  'nemo-placement-grid.c',
  'nemo-places-tree-view.c',
  'nemo-program-choosing.c',
  'nemo-progress-info-manager.c',
  'nemo-progress-info.c',
  'nemo-query.c',
  'nemo-recent.c',
  'nemo-saved-search-file.c',
  'nemo-search-directory-file.c',
  'nemo-search-directory.c',
  'nemo-search-engine-simple.c',
  'nemo-search-engine.c',
  'nemo-selection-canvas-item.c',
  'nemo-separator-action.c',
  'nemo-signaller.c',
  'nemo-thumbnails.c',
  'nemo-trash-monitor.c',
  'nemo-tree-view-drag-dest.c',
  'nemo-ui-utilities.c',
  'nemo-undo-manager.c',
  'nemo-undo-signal-handlers.c',
  'nemo-undo-transaction.c',
  'nemo-undo.c',
  'nemo-vfs-directory.c',
  'nemo-vfs-file.c',
  'nemo-widget-action.c',
  'nemo-widget-menu-item.c',
]

nemo_private_deps = [
  cinnamon, eel, gail, glib, gmodule, gtk, libexif, libxml, math, nemo_extension, x11,
]

if tracker_enabled
  nemo_private_sources += 'nemo-search-engine-tracker.c'
  nemo_private_deps += tracker_sparql
endif

if libselinux_enabled
  nemo_private_deps += libselinux
endif

nemo_private_lib = static_library('nemo-private',
  nemo_private_sources,
  dependencies: nemo_private_deps,
  include_directories: [ rootInclude, ],
  install: true,
  c_args: nemo_definitions,
)

nemo_private = declare_dependency(
  include_directories: include_directories('.'),
  link_with: [ nemo_private_lib ],
  dependencies: nemo_private_deps,
)

install_data('org.nemo.gschema.xml',
    install_dir: join_paths(get_option('datadir'), 'glib-2.0', 'schemas')
)
