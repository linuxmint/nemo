gresources = gnome.compile_resources(
  'nemo-resources', 'nemo.gresource.xml',
  source_dir: '.',
  c_name: 'nemo',
  install_header: false,
  install: false
)

layout_editor_gresources = gnome.compile_resources(
  'nemo-action-layout-editor-resources', 'nemo-action-layout-editor.gresource.xml',
  source_dir: '.',
  gresource_bundle: true,
  install: true,
  install_dir: nemoDataPath
)
