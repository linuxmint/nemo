include $(top_srcdir)/Makefile.shared

DIRS =				\
	eel			\
	libnemo-extension	\
	cut-n-paste-code 	\
	libnemo-private 	\
	src 			\
	test 			\
	po 			\
	data 			\
	icons 			\
	docs			\
	files			\
	$(NULL)

DIST_SUBDIRS = $(DIRS)

SUBDIRS = $(DIRS)

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING.EXTENSIONS			\
	COPYING-DOCS				\
	HACKING					\
	MAINTAINERS				\
	README.commits				\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	ChangeLog-20010201			\
	ChangeLog-20010420			\
	ChangeLog-20041029			\
	ChangeLog-20090417			\
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
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log NEMO_2_26_2.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
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
