NULL=

noinst_LTLIBRARIES = libegg.la

AM_CPPFLAGS = $(WARN_CFLAGS) $(BASE_CFLAGS)

EGG_TREE_DND_FILES = 		\
	eggtreemultidnd.c	\
	eggtreemultidnd.h	\
	$(NULL)

libegg_la_SOURCES = 		\
	$(EGG_TREE_DND_FILES)	\
	$(NULL)

libegg_la_CFLAGS =				\
	$(BASE_CFLAGS)			\
	$(WARN_CFLAGS)			\
	$(DISABLE_DEPRECATED)

libegg_la_LIBADD = 	\
	$(BASE_LIBS)

EXTRA_DIST = 			\
	update-from-egg.sh	\
	$(NULL)

EGG_TREE_DND_DIR = $(srcdir)/../../../libegg/libegg/treeviewutils

regenerate-built-sources:
	EGGFILES="$(EGG_TREE_DND_FILES)" EGGDIR="$(EGG_TREE_DND_DIR)" $(srcdir)/update-from-egg.sh
