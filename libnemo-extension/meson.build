
nemo_extension_sources = [
  'nemo-column-provider.c',
  'nemo-column.c',
  'nemo-extension-i18n.h',
  'nemo-extension-types.c',
  'nemo-extension-private.h',
  'nemo-file-info.c',
  'nemo-info-provider.c',
  'nemo-location-widget-provider.c',
  'nemo-menu-item.c',
  'nemo-menu-provider.c',
  'nemo-name-and-desc-provider.c',
  'nemo-property-page-provider.c',
  'nemo-property-page.c',
  'nemo-menu.c',
  'nemo-simple-button.c',
]

nemo_extension_headers = [
  'nemo-column-provider.h',
  'nemo-column.h',
  'nemo-extension-types.h',
  'nemo-file-info.h',
  'nemo-info-provider.h',
  'nemo-location-widget-provider.h',
  'nemo-menu-item.h',
  'nemo-menu-provider.h',
  'nemo-name-and-desc-provider.h',
  'nemo-property-page-provider.h',
  'nemo-property-page.h',
  'nemo-menu.h',
  'nemo-simple-button.h',
]

nemo_extension_incdir = include_directories('.')

nemo_extension_deps = [ glib, gtk, eel ]

nemo_extension_lib = shared_library('nemo-extension',
  nemo_extension_sources,
  dependencies: nemo_extension_deps,
  include_directories: [ rootInclude, ],
  install: true,
  version: '1.4.0',
  link_args: [ '-Wl,-no-undefined' ],
)

install_headers(nemo_extension_headers,
  subdir: join_paths('nemo', 'libnemo-extension')
)

nemo_extension = declare_dependency(
  include_directories: include_directories('.'),
  link_with: [ nemo_extension_lib ],
  dependencies: nemo_extension_deps,
)

gnome.generate_gir(nemo_extension_lib,
  sources: nemo_extension_sources + nemo_extension_headers,
  nsversion: '3.0',
  namespace: 'Nemo',
  includes: [ 'Gtk-3.0', 'Gio-2.0', 'GLib-2.0', ],
  include_directories: [ rootInclude, ],
  install: true,
)

pkgconfig.generate(filebase: 'libnemo-extension',
  name: 'libnemo-extension',
  description: 'A library to create Nemo view extensions',
  version: meson.project_version(),
  requires: [
    'gio-2.0',
    'glib-2.0',
    'gtk+-3.0',
  ],
  libraries: [
    '-L${libdir}',
    '-lnemo-extension',
  ],
  subdirs: [
    'nemo',
  ],
  variables: [
    'extensiondir=@0@'.format(nemoExtensionPath)
  ],
)
