CLEANFILES = $(NULL)

include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus-extension.la

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

libnautilus_extension_la_LDFLAGS=\
	-version-info @NAUTILUS_EXTENSION_VERSION_INFO@ \
	-no-undefined \
	$(NULL)

libnautilus_extension_la_LIBADD = $(BASE_LIBS)

libnautilus_extension_includedir=$(includedir)/nautilus/libnautilus-extension

libnautilus_extension_include_HEADERS=		\
	nautilus-column-provider.h              \
	nautilus-column.h                       \
	nautilus-extension-types.h              \
	nautilus-file-info.h                    \
	nautilus-info-provider.h                \
	nautilus-location-widget-provider.h     \
	nautilus-menu-item.h                    \
	nautilus-menu-provider.h                \
	nautilus-property-page-provider.h       \
	nautilus-property-page.h                \
	nautilus-menu.h                         \
	$(NULL)

libnautilus_extension_la_SOURCES=			\
	nautilus-column-provider.c                      \
	nautilus-column.c                               \
	nautilus-extension-i18n.h                       \
	nautilus-extension-types.c                      \
	nautilus-extension-private.h			\
	nautilus-file-info.c                            \
	nautilus-info-provider.c                        \
	nautilus-location-widget-provider.c             \
	nautilus-menu-item.c                            \
	nautilus-menu-provider.c                        \
	nautilus-property-page-provider.c               \
	nautilus-property-page.c                        \
	nautilus-menu.c                                 \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libnautilus-extension.pc

EXTRA_DIST = \
	libnautilus-extension.pc.in \
	$(NULL)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_files =					\
	$(libnautilus_extension_include_HEADERS)	\
	$(libnautilus_extension_la_SOURCES)
Nautilus-3.0.gir: libnautilus-extension.la Makefile
Nautilus_3_0_gir_INCLUDES = Gtk-3.0 Gio-2.0 GLib-2.0
Nautilus_3_0_gir_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS)
Nautilus_3_0_gir_LIBS = libnautilus-extension.la
Nautilus_3_0_gir_FILES = $(addprefix $(srcdir)/, $(introspection_files))
INTROSPECTION_GIRS += Nautilus-3.0.gir

girdir = $(datadir)/gir-1.0/
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif
