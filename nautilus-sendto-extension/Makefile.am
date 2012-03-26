include $(top_srcdir)/Makefile.shared

INCLUDES=\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BASE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

nautilus_extensiondir = $(libdir)/nautilus/extensions-3.0
nautilus_extension_LTLIBRARIES = libnautilus-sendto.la
libnautilus_sendto_la_SOURCES =		\
	nautilus-nste.c			\
	nautilus-nste.h			\
	nautilus-sendto-module.c

libnautilus_sendto_la_LDFLAGS = -module -avoid-version -no-undefined
libnautilus_sendto_la_LIBADD  = \
  $(top_builddir)/libnautilus-extension/libnautilus-extension.la \
  $(BASE_LIBS)
