NULL=

lib_LTLIBRARIES=libe-paned.la

INCLUDES = \
	-I$(top_builddir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(NULL)

libe_paned_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(NULL)

libe_panedincludedir=$(includedir)/libe-paned

libe_paned_la_SOURCES = \
	e-hpaned.c \
	e-paned.c \
	e-vpaned.c \
	$(NULL)

libe_panedinclude_HEADERS = 

# Everything is private for now
noinst_HEADERS =\
	e-hpaned.h \
	e-paned.h \
	e-vpaned.h \
	$(NULL)
