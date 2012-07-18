NULL=

INCLUDES =\
	-I$(top_srcdir) \
	$(BASE_CFLAGS) \
	$(COMMON_CFLAGS) \
	$(NEMO_CFLAGS) \
	$(WARNING_CFLAGS) \
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
	test-eel-editable-label	\
	$(NULL)

test_nemo_copy_SOURCES = test-copy.c test.c

test_nemo_search_engine_SOURCES = test-nemo-search-engine.c 

test_nemo_directory_async_SOURCES = test-nemo-directory-async.c

EXTRA_DIST = \
	test.h \
	$(NULL)
