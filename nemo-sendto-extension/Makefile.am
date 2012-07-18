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
nemo_extension_LTLIBRARIES = libnemo-sendto.la
libnemo_sendto_la_SOURCES =		\
	nemo-nste.c			\
	nemo-nste.h			\
	nemo-sendto-module.c

libnemo_sendto_la_LDFLAGS = -module -avoid-version -no-undefined
libnemo_sendto_la_LIBADD  = \
  $(top_builddir)/libnemo-extension/libnemo-extension.la \
  $(BASE_LIBS)
