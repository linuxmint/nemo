include $(top_srcdir)/Makefile.shared

DIRS =				\
	eel			\
	libnautilus-extension	\
	cut-n-paste-code 	\
	libnautilus-private 	\
	src 			\
	test 			\
	po 			\
	data 			\
	icons 			\
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
	COPYING-DOCS				\
	HACKING					\
	MAINTAINERS				\
	README.commits				\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(NULL)

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --enable-gtk-doc

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

distclean-local:
	if test "$(srcdir)" = "."; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log NAUTILUS_2_26_2.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog
