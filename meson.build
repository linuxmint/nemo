# Meson build file

# https://github.com/linuxmint/nemo
project('nemo', 'c', version : '6.4.5', meson_version : '>=0.56.0')

# 1. If the library code has changed at all since last release, then increment revision.
# 2. If any interfaces have been added, then increment current and set revision to 0.
# Interface break is not allowed.
nemo_extension_current  = 5
nemo_extension_revision = 0

# We need to decrement current by one in the calculation of the age because
# the library was started with version "1:0:0" instead of "0:0:0"
NEMO_EXTENSION_VERSION_INFO = '@0@:@1@:@2@'.format(
  nemo_extension_current, nemo_extension_revision, nemo_extension_current - 1
)

################################################################################
conf = configuration_data()

gnome = import('gnome')
pkgconfig = import('pkgconfig')

cc = meson.get_compiler('c')
prefix = get_option('prefix')
buildtype = get_option('buildtype')

# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', meson.project_version())

check_headers = [
  'malloc.h',
  'sys/mount.h',
  'sys/param.h',
  'sys/vfs.h',
  'X11/XF86keysym.h',
]

foreach h : check_headers
  conf.set10('HAVE_' + h.underscorify().to_upper(), cc.has_header(h))
endforeach

conf.set10('HAVE_MALLOPT', cc.has_function('mallopt', prefix: '#include <malloc.h>'))


if not get_option('deprecated_warnings')
  add_global_arguments([
      '-Wno-deprecated-declarations',
      '-Wno-deprecated',
      '-Wno-declaration-after-statement',
      '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    ],
    language: 'c',
  )
endif

if buildtype in ['debug', 'debugoptimized']
    conf.set('ENABLE_DEBUG', true)
endif

if buildtype == 'debug'
    add_global_arguments('-fno-omit-frame-pointer', language: 'c')
endif

################################################################################
# Find dependencies

glib_version = '>=2.45.7'

math    = cc.find_library('m', required: true)

gtk     = dependency('gtk+-3.0',                    version: '>=3.10.0')
gio     = dependency('gio-2.0',                     version: glib_version)
gio_unix= dependency('gio-unix-2.0',                version: glib_version)
glib    = dependency('glib-2.0',                    version: glib_version)
gmodule = dependency('gmodule-no-export-2.0',       version: glib_version)
gobject = dependency('gobject-2.0',                 version: '>=2.0')
go_intr = dependency('gobject-introspection-1.0',   version: '>=1.0')
json    = dependency('json-glib-1.0',               version: '>=1.6')
vte     = dependency('vte-2.91',                    version: '>=0.52.0')

cinnamon= dependency('cinnamon-desktop',            version: '>=4.8.0')
gail    = dependency('gail-3.0')
x11     = dependency('x11')
xapp    = dependency('xapp',                        version: '>=2.0.0')

# Facultative dependencies

trackerChoice = get_option('tracker')
tracker_enabled = false
if trackerChoice != 'false'
  trackerRequired = (trackerChoice == 'true')
  # Check all the possible versions
    tracker_sparql = dependency('tracker-sparql-3.0',  required: false)
  if not tracker_sparql.found()
    tracker_sparql = dependency('tracker-sparql-2.0',  required: false)
  endif
  if not tracker_sparql.found()
    tracker_sparql = dependency('tracker-sparql-1.0',  required: false)
  endif
  if not tracker_sparql.found()
    tracker_sparql = dependency('tracker-sparql-0.18', required: false)
  endif
  if not tracker_sparql.found()
    tracker_sparql = dependency('tracker-sparql-0.16', required: trackerRequired)
  endif

  tracker_enabled = trackerRequired or tracker_sparql.found()
endif
conf.set('ENABLE_TRACKER', tracker_enabled)


gtkdoc_enabled = get_option('gtk_doc')
if gtkdoc_enabled
  find_program('gtkdoc-scan', required: true)
endif


libexif_enabled = get_option('exif')
if libexif_enabled
  libexif = dependency('libexif',                   version: '>=0.6.20')
endif
conf.set('HAVE_EXIF', libexif_enabled)

exempi_enabled = get_option('xmp')
if exempi_enabled
  exempi = dependency('exempi-2.0',                 version: '>=2.2.0')
endif
conf.set('HAVE_EXEMPI', exempi_enabled)

libselinux_enabled = get_option('selinux')
if libselinux_enabled
  libselinux = dependency('libselinux',             version: '>=2.0')
endif
conf.set('HAVE_SELINUX', libselinux_enabled)

# make sure pango development files are installed
pango = dependency('pango', version: '>=1.40.0')
# check for newer pango for necessary workarounds
new_pango = dependency('pango', version: '>=1.44.0', required: false)
if new_pango.found()
  conf.set('HAVE_PANGO_144', true)
else
  message('................using pango @0@ instead'.format(new_pango.version()))
endif

enableEmptyView = get_option('empty_view')
conf.set10('ENABLE_EMPTY_VIEW', enableEmptyView)

conf.set_quoted('GETTEXT_PACKAGE', 'nemo')
conf.set_quoted('LOCALE_DIR',  join_paths(get_option('prefix'), get_option('localedir')))
conf.set_quoted('LOCALEDIR',   join_paths(get_option('prefix'), get_option('localedir')))
conf.set('ENABLE_NLS', cc.has_header('libintl.h'))
conf.set('HAVE_GETTEXT', true)
conf.set('HAVE_LOCALE_H', cc.has_header('locale.h'))

configure_file(
  input : 'config.h.meson.in',
  output: 'config.h',
  configuration: conf
)

################################################################################

rootInclude = include_directories('.')
nemoDataPath      = join_paths(get_option('prefix'), get_option('datadir'), 'nemo')
libExecPath       = join_paths(get_option('prefix'), get_option('libexecdir'))
# Keep this constant, in case some extensions are behind in being updated...
nemoExtensionPath = join_paths(get_option('prefix'), get_option('libdir'), 'nemo', 'extensions-3.0')

nemo_definitions =  [
  '-DNEMO_DATADIR="@0@"'.format(nemoDataPath),
  '-DNEMO_EXTENSIONDIR="@0@"'.format(nemoExtensionPath),
  '-DLIBEXECDIR="@0@"'.format(libExecPath),
  '-DG_LOG_DOMAIN="Nemo"'
]

po_subdir = join_paths(meson.project_source_root(), 'po')

subdir('install-scripts')
subdir('cut-n-paste-code/libegg')
subdir('data')
subdir('eel')
subdir('files')
subdir('gresources')
subdir('libnemo-extension')
subdir('libnemo-private')
# subdir('po')
subdir('src')
subdir('search-helpers')
subdir('test')
subdir('docs')
subdir('action-layout-editor')

message('\n'.join(['',
'        @0@-@1@'.format(meson.project_name(), meson.project_version()),
'',
'    prefix:                 @0@'.format(prefix),
'    source code location:   @0@'.format(meson.project_source_root()),
'    compiler:               @0@'.format(cc.get_id()),
'    libexif support:        @0@'.format(libexif_enabled),
'    exempi  support:        @0@'.format(exempi_enabled),
'    Tracker support:        @0@'.format(tracker_enabled),
'    Wayland support:        @0@'.format(cc.has_header('gdk/gdkwayland.h', dependencies: gtk)),
'',
'    nemo-extension documentation: @0@'.format(gtkdoc_enabled),
'    nemo-extension introspection: @0@'.format(true),
'',
'    debugging support:      @0@'.format(buildtype),
'    perf profiling support: @0@'.format(buildtype == 'debug'),
'',
]))
