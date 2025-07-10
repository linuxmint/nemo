dbusBuiltSources = gnome.gdbus_codegen(
  'nemo-freedesktop-generated',
  join_paths(meson.project_source_root(), 'data', 'freedesktop-dbus-interfaces.xml'),
  interface_prefix: 'org.freedesktop.',
  namespace: 'NemoFreedesktop',
  object_manager: true
)

nemoBuiltSources = gnome.gdbus_codegen(
  'nemo-cinnamon-dbus',
  join_paths(meson.project_source_root(), 'data', 'org.Cinnamon.xml'),
  interface_prefix: 'org.',
  namespace: 'Nemo',
  object_manager: true
)

nemoCommon_sources = [
  dbusBuiltSources,
  nemoBuiltSources,
  'nemo-action-config-widget.c',
  'nemo-application.c',
  'nemo-blank-desktop-window.c',
  'nemo-bookmark-list.c',
  'nemo-bookmarks-window.c',
  'nemo-config-base-widget.c',
  'nemo-connect-server-dialog-nonmain.c',
  'nemo-connect-server-dialog.c',
  'nemo-connect-server-operation.c',
  'nemo-desktop-icon-grid-view.c',
  'nemo-desktop-item-properties.c',
  'nemo-desktop-manager.c',
  'nemo-desktop-overlay.c',
  'nemo-desktop-window.c',
  'nemo-error-reporting.c',
  'nemo-extension-config-widget.c',
  'nemo-file-management-properties.c',
  'nemo-floating-bar.c',
  'nemo-freedesktop-dbus.c',
  'nemo-icon-view-container.c',
  'nemo-icon-view-grid-container.c',
  'nemo-icon-view.c',
  'nemo-image-properties-page.c',
  'nemo-interesting-folder-bar.c',
  'nemo-list-model.c',
  'nemo-list-view.c',
  'nemo-location-bar.c',
  'nemo-location-entry.c',
  'nemo-mime-actions.c',
  'nemo-navigation-action.c',
  'nemo-navigation-state.c',
  'nemo-notebook.c',
  'nemo-pathbar.c',
  'nemo-places-sidebar.c',
  'nemo-plugin-manager.c',
  'nemo-previewer.c',
  'nemo-progress-info-widget.c',
  'nemo-progress-ui-handler.c',
  'nemo-properties-window.c',
  'nemo-query-editor.c',
  'nemo-script-config-widget.c',
  'nemo-self-check-functions.c',
  'nemo-statusbar.c',
  'nemo-terminal-widget.c',
  'nemo-thumbnail-problem-bar.c',
  'nemo-toolbar.c',
  'nemo-trash-bar.c',
  'nemo-tree-sidebar-model.c',
  'nemo-tree-sidebar.c',
  'nemo-view-dnd.c',
  'nemo-view-factory.c',
  'nemo-view.c',
  'nemo-window-bookmarks.c',
  'nemo-window-manage-views.c',
  'nemo-window-menus.c',
  'nemo-window-pane.c',
  'nemo-window-slot-dnd.c',
  'nemo-window-slot.c',
  'nemo-window.c',
  'nemo-x-content-bar.c',
]

nemoWindow_sources = [
  'nemo-main-application.c',
  'nemo-main.c',
]
nemoWindow_headers = [
  'nemo-main-application.h',
]

nemoDesktop_sources = [
  'nemo-desktop-application.c',
  'nemo-desktop-icon-view.c',
  'nemo-desktop-main.c',
]
nemoDesktop_headers = [
  'nemo-desktop-application.h',
  'nemo-desktop-overlay.h',
  'nemo-desktop-icon-view.h',
]


if enableEmptyView
  nemoCommon_sources += 'nemo-empty-view.c'
endif

nemo_deps = [ cinnamon, gail, glib, gtk, math,
  egg, nemo_extension, nemo_private, xapp, vte ]

if exempi_enabled
  nemo_deps += exempi
endif

if libexif_enabled
  nemo_deps += libexif
endif

nemo = executable('nemo',
  nemoCommon_sources + nemoWindow_sources,
  include_directories: [ rootInclude ],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  install: true
)

nemoDesktop = executable('nemo-desktop',
  nemoCommon_sources + nemoDesktop_sources,
  include_directories: [ rootInclude],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  install: true
)

nemo_autorun_software = executable('nemo-autorun-software',
  [ 'nemo-autorun-software.c' ],
  include_directories: [ rootInclude, ],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  install: true
)

nemo_connect_server = executable('nemo-connect-server',
  [
    'nemo-bookmark-list.c',
    'nemo-connect-server-dialog.c',
    'nemo-connect-server-dialog-main.c',
    'nemo-connect-server-operation.c'
  ],
  include_directories: [ rootInclude, ],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  install: true
)

nemo_open_with = executable('nemo-open-with',
  [ 'nemo-open-with-main.c' ],
  include_directories: [ rootInclude, ],
  c_args: nemo_definitions,
  dependencies: nemo_deps,
  install: true
)

# LibExecs
nemo_extensions_list = executable('nemo-extensions-list',
  [ 'nemo-extensions-list.c' ],
  include_directories: [ rootInclude, ],
  c_args: nemo_definitions,
  dependencies: [ nemo_deps ],
  install: true,
  install_dir: libExecPath,
)
