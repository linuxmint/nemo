
docConf = configuration_data()
docConf.set('NEMO_EXTENSION_VERSION_INFO', NEMO_EXTENSION_VERSION_INFO)

configure_file(
  input : 'version.xml.in',
  output: 'version.xml',
  configuration: docConf
)

gnome.gtkdoc('libnemo-extension',
  main_xml: 'libnemo-extension-docs.xml',
  src_dir: nemo_extension_incdir,
  dependencies: [ nemo_extension ],
  install: true,
  install_dir: 'libnemo-extension',
  scan_args: [ '--deprecated-guards="G_DISABLE_DEPRECATED"' ],
  scanobjs_args: [ '--type-init-func="g_type_init()"', '--ignore-headers="config.h"' ],
  mkdb_args: [ '--xml-mode', '--output-format=xml' ],
)
