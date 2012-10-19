## Process this file with automake to produce Makefile.in

include $(top_srcdir)/Makefile.shared

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

nemo_extensiondir = $(libdir)/nemo/extensions-3.0

nemo_extension_LTLIBRARIES = libnemo-wallpaper.la

libnemo_wallpaper_la_SOURCES = \
	nemo-nwe.c\
	nemo-nwe.h\
	nemo-wallpaper-command.c

libnemo_wallpaper_la_LDFLAGS = \
	-module -avoid-version -no-undefined

libnemo_wallpaper_la_LIBADD = \
	$(top_builddir)/libnemo-extension/libnemo-extension.la \
	$(BASE_LIBS)
