
eel_sources = [
  'eel-accessibility.c',
  'eel-art-extensions.c',
  'eel-canvas.c',
  'eel-debug.c',
  'eel-editable-label.c',
  'eel-gdk-extensions.c',
  'eel-glib-extensions.c',
  'eel-gnome-extensions.c',
  'eel-graphic-effects.c',
  'eel-gtk-extensions.c',
  'eel-lib-self-check-functions.c',
  'eel-self-checks.c',
  'eel-stock-dialogs.c',
  'eel-string.c',
  'eel-vfs-extensions.c',
]


eel_deps = [ gtk, libxml, gail ]

eel_lib = static_library('eel',
  eel_sources,
  dependencies: eel_deps,
  include_directories: [ rootInclude, ],
  install: false,
  c_args: '-DG_LOG_DOMAIN="Eel"'
)

eel = declare_dependency(
  include_directories: include_directories('.'),
  link_with: [
    eel_lib
  ],
  dependencies: eel_deps,
)


test('Eel test',
  executable('check-program',
    'check-program.c',
    dependencies: eel,
    include_directories: [ rootInclude, ],
    install: false
  ),
  args: ['--g-fatal-warnings', '--sm-disable']
)
