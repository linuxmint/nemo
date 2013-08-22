NULL=

noinst_LTLIBRARIES=libeel-2.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Eel\"				\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(BASE_CFLAGS)					\
	$(COMMON_CFLAGS)				\
	$(WARNING_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	-DDATADIR=\""$(datadir)"\"			\
	-DSOURCE_DATADIR=\""$(top_srcdir)/data"\"	\
	-DLOCALEDIR=\""$(prefix)/${DATADIRNAME}/cinnamon/locale"\"	\
	$(NULL)

libeel_2_la_LDFLAGS =				\
	-no-undefined				\
	$(NULL)

libeel_2_la_LIBADD = 				\
	$(BASE_LIBS)				\
	$(COMMON_LIBS)				\
	$(RENDER_LIBS)				\
	-lm					\
	$(NULL)

eel_headers =					\
	eel-accessibility.h			\
	eel-art-extensions.h			\
	eel-canvas.h				\
	eel-debug.h				\
	eel-editable-label.h			\
	eel-gdk-extensions.h			\
	eel-glib-extensions.h			\
	eel-gnome-extensions.h			\
	eel-graphic-effects.h			\
	eel-gtk-extensions.h			\
	eel-self-checks.h			\
	eel-stock-dialogs.h			\
	eel-string.h				\
	eel-vfs-extensions.h			\
	eel.h					\
	$(NULL)

libeel_2_la_SOURCES =				\
	eel-accessibility.c			\
	eel-art-extensions.c			\
	eel-canvas.c				\
	eel-debug.c				\
	eel-editable-label.c			\
	eel-gdk-extensions.c			\
	eel-glib-extensions.c			\
	eel-gnome-extensions.c			\
	eel-graphic-effects.c			\
	eel-gtk-extensions.c			\
	eel-lib-self-check-functions.c		\
	eel-self-checks.c			\
	eel-stock-dialogs.c			\
	eel-string.c				\
	eel-vfs-extensions.c			\
	eel-lib-self-check-functions.h		\
	$(eel_headers)				\
	$(NULL)

noinst_PROGRAMS = check-program

check_program_SOURCES = check-program.c
check_program_DEPENDENCIES = libeel-2.la
check_program_LDADD = $(EEL_LIBS)
check_program_LDFLAGS =	$(check_program_DEPENDENCIES) -lm

TESTS = check-eel

EXTRA_DIST =					\
	check-eel				\
	$(NULL)
