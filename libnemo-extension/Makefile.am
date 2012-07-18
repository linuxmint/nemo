CLEANFILES = $(NULL)

include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnemo-extension.la

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

libnemo_extension_la_LDFLAGS=\
	-version-info @NEMO_EXTENSION_VERSION_INFO@ \
	-no-undefined \
	$(NULL)

libnemo_extension_la_LIBADD = $(BASE_LIBS)

libnemo_extension_includedir=$(includedir)/nemo/libnemo-extension

libnemo_extension_include_HEADERS=		\
	nemo-column-provider.h              \
	nemo-column.h                       \
	nemo-extension-types.h              \
	nemo-file-info.h                    \
	nemo-info-provider.h                \
	nemo-location-widget-provider.h     \
	nemo-menu-item.h                    \
	nemo-menu-provider.h                \
	nemo-property-page-provider.h       \
	nemo-property-page.h                \
	nemo-menu.h                         \
	$(NULL)

libnemo_extension_la_SOURCES=			\
	nemo-column-provider.c                      \
	nemo-column.c                               \
	nemo-extension-i18n.h                       \
	nemo-extension-types.c                      \
	nemo-extension-private.h			\
	nemo-file-info.c                            \
	nemo-info-provider.c                        \
	nemo-location-widget-provider.c             \
	nemo-menu-item.c                            \
	nemo-menu-provider.c                        \
	nemo-property-page-provider.c               \
	nemo-property-page.c                        \
	nemo-menu.c                                 \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libnemo-extension.pc

EXTRA_DIST = \
	libnemo-extension.pc.in \
	$(NULL)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_files =					\
	$(libnemo_extension_include_HEADERS)	\
	$(libnemo_extension_la_SOURCES)
Nemo-3.0.gir: libnemo-extension.la Makefile
Nemo_3_0_gir_INCLUDES = Gtk-3.0 Gio-2.0 GLib-2.0
Nemo_3_0_gir_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS)
Nemo_3_0_gir_LIBS = libnemo-extension.la
Nemo_3_0_gir_FILES = $(addprefix $(srcdir)/, $(introspection_files))
INTROSPECTION_GIRS += Nemo-3.0.gir

girdir = $(datadir)/gir-1.0/
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif
