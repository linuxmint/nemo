
egg_lib = static_library('egg',
  'eggtreemultidnd.c',
  dependencies: [
    gtk,
  ],
  install: false
)

egg = declare_dependency(
  include_directories: include_directories('..'),
  link_with: egg_lib,
  dependencies: [
    gtk,
  ],
)
