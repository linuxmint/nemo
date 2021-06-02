libgsf = dependency('libgsf-1', required: true)

mso_to_txt_sources = [
  'nemo-mso-to-txt.c'
]

watcher = executable('nemo-mso-to-txt',
  mso_to_txt_sources,
  dependencies: [libgsf, gio, glib],
  install: true
)

install_data(
    'mso.nemo_search_helper',
    install_dir: join_paths(nemoDataPath, 'search-helpers')
)

subdir('third-party')