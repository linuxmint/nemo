conf = configuration_data()
conf.set_quoted('PKG_DATADIR', nemoDataPath)
conf.set_quoted('LOCALE_DIR', join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('VERSION', meson.project_version())

config_py = configure_file(
  input: 'leconfig.py.in',
  output: 'leconfig.py',
  configuration: conf,
  install: true,
  install_dir: nemoDataPath / 'layout-editor',
)

bin_conf = configuration_data()
bin_conf.set('PKG_DATADIR', nemoDataPath)

bin = configure_file(
  input: 'nemo-action-layout-editor.in',
  output: 'nemo-action-layout-editor',
  configuration: bin_conf,
  install: true,
  install_dir: get_option('bindir'),
  install_mode: 'rwxr-xr-x'
)

install_data(
    'nemo-action-layout-editor.py',
    install_dir: nemoDataPath / 'layout-editor',
    install_mode: 'rwxr-xr-x'
)

install_data(
    'nemo-action-layout-editor.glade',
    install_dir: nemoDataPath / 'layout-editor'
)
