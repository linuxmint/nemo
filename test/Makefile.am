NULL=

INCLUDES =\
	-I$(top_srcdir) \
	$(BASE_CFLAGS) \
	$(COMMON_CFLAGS) \
	$(NEMO_CFLAGS) \
	$(WARNING_CFLAGS) \
	`pkg-config --cflags zeitgeist-1.0` \
	-DVERSION="\"$(VERSION)\"" \
	-DNEMO_DATADIR=\""$(datadir)/nemo"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(NULL)

LDADD =\
	$(top_builddir)/libnemo-private/libnemo-private.la \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NEMO_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nemo-search-engine \
	test-nemo-directory-async \
	test-nemo-copy \
	test-nemo-zeitgeist \
	test-eel-editable-label	\
	$(NULL)

test_nemo_copy_SOURCES = test-copy.c test.c

test_nemo_search_engine_SOURCES = test-nemo-search-engine.c 

test_nemo_directory_async_SOURCES = test-nemo-directory-async.c

test_nemo_zeitgeist_SOURCES = test-nemo-zeitgeist.c

test_nemo_zeitgeist_LDCONFIG = `pkg-config --libs zeitgeist-1.0`

EXTRA_DIST = \
	test.h \
	$(NULL)
