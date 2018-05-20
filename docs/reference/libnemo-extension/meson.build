
docConf = configuration_data()
docConf.set('NEMO_EXTENSION_VERSION_INFO', NEMO_EXTENSION_VERSION_INFO)

configure_file(
  input : 'version.xml.in',
  output: 'version.xml',
  configuration: docConf
)

gnome.gtkdoc('libnemo-extension',
  main_xml: 'libnemo-extension-docs.xml',
  src_dir: join_paths(meson.source_root(), 'libnemo-extension'),
  dependencies: nemo_extension,
  install: true,
  gobject_typesfile: 'libnemo-extension.types',
  install_dir: 'libnemo-extension',
  scan_args: [
    '--rebuild-types',
    '--ignore-headers="config.h"' ]
)
