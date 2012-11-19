include $(top_srcdir)/Makefile.shared

bin_PROGRAMS=					\
	nemo				\
	nemo-autorun-software		\
	nemo-connect-server			\
	$(NULL)

libexec_PROGRAMS=				\
	nemo-convert-metadata		\
	$(NULL)

INCLUDES =							\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/cut-n-paste-code			\
	-I$(top_builddir)/libnemo-private 			\
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NEMO_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DNEMO_DATADIR=\""$(datadir)/nemo"\" 		\
	-DUIDIR=\""$(datadir)/nemo/ui"\" 		\
	-DNEMO_PIXMAPDIR=\""$(datadir)/pixmaps/nemo"\" 	\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED_CFLAGS)                            \
	$(NULL)

LDADD =\
	$(top_builddir)/libnemo-private/libnemo-private.la  \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NEMO_LIBS) \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(NULL)

dbus_built_sources = nemo-freedesktop-generated.c nemo-freedesktop-generated.h
$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/freedesktop-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.freedesktop.					\
		--c-namespace NemoFreedesktop					\
		--c-generate-object-manager						\
		--generate-c-code nemo-freedesktop-generated			\
		$(top_srcdir)/data/freedesktop-dbus-interfaces.xml			\
		$(NULL)

UI_FILES = \
	nemo-bookmarks-window.ui		\
	nemo-file-management-properties.ui	\
	nemo-icon-view-ui.xml		\
	nemo-toolbar-ui.xml			\
	nemo-directory-view-ui.xml		\
	nemo-desktop-icon-view-ui.xml	\
	nemo-list-view-ui.xml		\
	nemo-shell-ui.xml			\
	$(NULL)

nemo-resources.c: nemo.gresource.xml $(UI_FILES)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name nemo $(srcdir)/nemo.gresource.xml

nemo-resources.h: nemo.gresource.xml
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name nemo $(srcdir)/nemo.gresource.xml


BUILT_SOURCES = \
	$(dbus_built_sources) \
	nemo-resources.c \
	nemo-resources.h \
	$(NULL)

nemo_SOURCES = \
	nemo-actions.h			\
	nemo-application.c			\
	nemo-application.h			\
	nemo-bookmark-list.c		\
	nemo-bookmark-list.h		\
	nemo-bookmarks-window.c		\
	nemo-bookmarks-window.h		\
	nemo-connect-server-dialog.c	\
	nemo-connect-server-dialog.h	\
	nemo-connect-server-dialog-nonmain.c \
	nemo-connect-server-operation.c	\
	nemo-connect-server-operation.h	\
	nemo-desktop-icon-view.c		\
	nemo-desktop-icon-view.h		\
	nemo-desktop-item-properties.c	\
	nemo-desktop-item-properties.h	\
	nemo-desktop-window.c		\
	nemo-desktop-window.h		\
	nemo-error-reporting.c		\
	nemo-error-reporting.h		\
	nemo-file-management-properties.c	\
	nemo-file-management-properties.h	\
	nemo-floating-bar.c			\
	nemo-floating-bar.h			\
	nemo-freedesktop-dbus.c		\
	nemo-freedesktop-dbus.h		\
	nemo-icon-view.c			\
	nemo-icon-view.h			\
	nemo-icon-view-container.c		\
	nemo-icon-view-container.h		\
	nemo-image-properties-page.c	\
	nemo-image-properties-page.h	\
	nemo-list-model.c			\
	nemo-list-model.h			\
	nemo-list-view-private.h 		\
	nemo-list-view.c			\
	nemo-list-view.h 			\
	nemo-location-bar.c         	\
	nemo-location-bar.h			\
	nemo-location-entry.c               \
	nemo-location-entry.h               \
	nemo-main.c 			\
	nemo-mime-actions.c 		\
	nemo-mime-actions.h 		\
	nemo-navigation-action.c		\
	nemo-navigation-action.h		\
	nemo-navigation-state.c		\
	nemo-navigation-state.h		\
	nemo-notebook.c			\
	nemo-notebook.h			\
	nemo-pathbar.c			\
	nemo-pathbar.h			\
	nemo-places-sidebar.c		\
	nemo-places-sidebar.h		\
	nemo-previewer.c			\
	nemo-previewer.h			\
	nemo-progress-info-widget.c		\
	nemo-progress-info-widget.h		\
	nemo-progress-ui-handler.c		\
	nemo-progress-ui-handler.h		\
	nemo-properties-window.c		\
	nemo-properties-window.h		\
	nemo-query-editor.c			\
	nemo-query-editor.h			\
	nemo-search-bar.c			\
	nemo-search-bar.h			\
	nemo-self-check-functions.c 	\
	nemo-self-check-functions.h 	\
    nemo-statusbar.c    \
    nemo-statusbar.h    \
	nemo-toolbar.c			\
	nemo-toolbar.h			\
	nemo-tree-sidebar.c			\
	nemo-tree-sidebar.h			\
	nemo-tree-sidebar-model.c		\
	nemo-tree-sidebar-model.h		\
	nemo-trash-bar.c			\
	nemo-trash-bar.h			\
	nemo-view.c				\
	nemo-view.h				\
	nemo-view-dnd.c			\
	nemo-view-dnd.h			\
	nemo-view-factory.c 		\
	nemo-view-factory.h 		\
	nemo-window-bookmarks.c		\
	nemo-window-bookmarks.h		\
	nemo-window-manage-views.c		\
	nemo-window-manage-views.h		\
	nemo-window-menus.c         	\
	nemo-window-menus.h         \
	nemo-window-pane.c	\
	nemo-window-pane.h	\
	nemo-window-private.h		\
	nemo-window-slot.c			\
	nemo-window-slot.h			\
	nemo-window-slot-dnd.c		\
	nemo-window-slot-dnd.h		\
	nemo-window.c			\
	nemo-window.h			\
	nemo-window-types.h			\
	nemo-x-content-bar.c		\
	nemo-x-content-bar.h		\
	$(NULL)

nodist_nemo_SOURCES = \
	$(BUILT_SOURCES) \
	$(NULL)

EMPTY_VIEW_SOURCES = \
	nemo-empty-view.c 			\
	nemo-empty-view.h

if ENABLE_EMPTY_VIEW
nemo_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

nemo_autorun_software_SOURCES= 			\
	nemo-autorun-software.c			\
	$(NULL)

nemo_connect_server_SOURCES= \
	nemo-bookmark-list.c		\
	nemo-bookmark-list.h		\
	nemo-connect-server-dialog.c	\
	nemo-connect-server-dialog.h	\
	nemo-connect-server-dialog-main.c	\
	nemo-connect-server-operation.c     \
	nemo-connect-server-operation.h     \
	$(NULL)

nemo_convert_metadata_SOURCES= \
	nemo-convert-metadata.c	\
	$(NULL)

TESTS=check-nemo

@INTLTOOL_SERVER_RULE@

CLEANFILES = \
	$(BUILT_SOURCES)			\
	$(server_DATA)				\
	$(NULL)

EXTRA_DIST =					\
	$(server_in_files)			\
	$(UI_FILES)				\
	nemo.gresource.xml			\
	check-nemo				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
