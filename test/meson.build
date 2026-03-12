
test('Search Engine test',
  executable('test-nemo-search-engine',
    [ 'test-nemo-search-engine.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Directory Async test',
  executable('test-nemo-directory-async',
    [ 'test-nemo-directory-async.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
  ),
  args: []
)

test('Copy test',
  executable('test-nemo-copy',
    [ 'test-copy.c', 'test.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ gtk, nemo_private ],
    c_args: nemo_definitions,
  ),
  args: []
)

# ---------------------------------------------------------------------------
# Dual-pane schema tests
# ---------------------------------------------------------------------------
# Compile our schema + any installed nemo/cinnamon schemas into the test
# build dir so GSETTINGS_SCHEMA_DIR points at a valid gschemas.compiled.
# Outputs land in meson.current_build_dir() (i.e. build/test/).
nemo_compiled_schemas = custom_target('nemo-compiled-schemas',
  output: 'gschemas.compiled',
  input:  join_paths(meson.project_source_root(), 'libnemo-private', 'org.nemo.gschema.xml'),
  command: [
    find_program('sh'), '-c',
    'set -e; D="@OUTDIR@";' +
    'cp /usr/share/glib-2.0/schemas/org.nemo*.xml     "$D/" 2>/dev/null || true;' +
    'cp /usr/share/glib-2.0/schemas/org.cinnamon*.xml "$D/" 2>/dev/null || true;' +
    'cp /usr/share/glib-2.0/schemas/org.gtk.*.xml     "$D/" 2>/dev/null || true;' +
    'cp @INPUT0@ "$D/org.nemo.gschema.xml";' +
    'glib-compile-schemas "$D"',
  ],
  build_by_default: false,
)

# Schema unit tests — no display, no D-Bus, no session required.
# Tests that our three new preference keys exist with correct defaults.
# This is the authoritative automated test for the dual-pane feature's schema.
test('Dual Pane schema test',
  executable('test-dual-pane-schema',
    [ 'test-dual-pane-schema.c' ],
    include_directories: [ rootInclude, ],
    dependencies: [ glib, gobject, gio ],
    c_args: nemo_definitions,
  ),
  args: [],
  suite: [ 'dual-pane' ],
  depends: [ nemo_compiled_schemas ],
  env: [ 'GSETTINGS_SCHEMA_DIR=' + meson.current_build_dir() ],
  timeout: 30,
)

# NOTE: Widget layout tests (test-dual-pane.c) require a full Nemo session:
# a running display, D-Bus session bus, and GVfs.  They are not run as part
# of the automated suite.  Run manually with Xvfb:
#
#   xvfb-run meson test -C build --suite dual-pane-widget
#
# when that suite is added in the future.
