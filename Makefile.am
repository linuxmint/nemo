include $(top_srcdir)/Makefile.shared

DIRS =				\
	eel			\
	libnautilus-extension	\
	libnautilus-private 	\
	src 			\
	test 			\
	po 			\
	data 			\
	docs			\
	$(NULL)

DIST_SUBDIRS = $(DIRS) nautilus-sendto-extension

SUBDIRS = $(DIRS)
if ENABLE_NST_EXTENSION
SUBDIRS += nautilus-sendto-extension
endif

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING.EXTENSIONS			\
	HACKING					\
	MAINTAINERS				\
	README.commits				\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	$(NULL)

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	`find "$(srcdir)" -type f -name Makefile.in -print`

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --enable-gtk-doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

-include $(top_srcdir)/git.mk
