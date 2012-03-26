include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/cut-n-paste-code		\
	$(BASE_CFLAGS)					\
	$(COMMON_CFLAGS)				\
	$(NAUTILUS_CFLAGS)				\
	$(WARNING_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(TRACKER_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 	\
	-DNAUTILUS_EXTENSIONDIR=\""$(libdir)/nautilus/extensions-3.0"\" \
	$(NULL)

dbus_built_sources = nautilus-generated.c nautilus-generated.h
$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.Nautilus.					\
		--c-namespace NautilusDBus						\
		--c-generate-object-manager						\
		--generate-c-code nautilus-generated					\
		$(top_srcdir)/data/dbus-interfaces.xml					\
		$(NULL)

BUILT_SOURCES = \
	$(dbus_built_sources) \
	$(NULL)

if ENABLE_TRACKER
nautilus_tracker_engine_sources = \
	nautilus-search-engine-tracker.c \
	nautilus-search-engine-tracker.h
endif

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/libegg/libegg.la \
	$(NULL)

libnautilus_private_la_LDFLAGS =	\
	-no-undefined			\
	$(NULL)

libnautilus_private_la_LIBADD =		\
	$(dependency_static_libs)	\
	$(SELINUX_LIBS)			\
	$(TRACKER_LIBS)			\
	$(top_builddir)/eel/libeel-2.la \
	$(top_builddir)/libnautilus-extension/libnautilus-extension.la \
	$(BASE_LIBS)			\
	$(COMMON_LIBS)			\
	$(NAUTILUS_LIBS)		\
	-lm				\
	$(NULL)

libnautilus_private_la_SOURCES = \
	nautilus-bookmark.c \
	nautilus-bookmark.h \
	nautilus-cell-renderer-text-ellipsized.c \
	nautilus-cell-renderer-text-ellipsized.h \
	nautilus-clipboard-monitor.c \
	nautilus-clipboard-monitor.h \
	nautilus-clipboard.c \
	nautilus-clipboard.h \
	nautilus-column-chooser.c \
	nautilus-column-chooser.h \
	nautilus-column-utilities.c \
	nautilus-column-utilities.h \
	nautilus-dbus-manager.c \
	nautilus-dbus-manager.h \
	nautilus-debug.c \
	nautilus-debug.h \
	nautilus-default-file-icon.c \
	nautilus-default-file-icon.h \
	nautilus-desktop-background.c \
	nautilus-desktop-background.h \
	nautilus-desktop-directory-file.c \
	nautilus-desktop-directory-file.h \
	nautilus-desktop-directory.c \
	nautilus-desktop-directory.h \
	nautilus-desktop-icon-file.c \
	nautilus-desktop-icon-file.h \
	nautilus-desktop-link-monitor.c \
	nautilus-desktop-link-monitor.h \
	nautilus-desktop-link.c \
	nautilus-desktop-link.h \
	nautilus-desktop-metadata.c \
	nautilus-desktop-metadata.h \
	nautilus-directory-async.c \
	nautilus-directory-notify.h \
	nautilus-directory-private.h \
	nautilus-directory.c \
	nautilus-directory.h \
	nautilus-dnd.c \
	nautilus-dnd.h \
	nautilus-entry.c \
	nautilus-entry.h \
	nautilus-file-attributes.h \
	nautilus-file-changes-queue.c \
	nautilus-file-changes-queue.h \
	nautilus-file-conflict-dialog.c \
	nautilus-file-conflict-dialog.h \
	nautilus-file-dnd.c \
	nautilus-file-dnd.h \
	nautilus-file-operations.c \
	nautilus-file-operations.h \
	nautilus-file-private.h \
	nautilus-file-queue.c \
	nautilus-file-queue.h \
	nautilus-file-utilities.c \
	nautilus-file-utilities.h \
	nautilus-file.c \
	nautilus-file.h \
	nautilus-generated.c \
	nautilus-generated.h \
	nautilus-global-preferences.c \
	nautilus-global-preferences.h \
	nautilus-icon-canvas-item.c \
	nautilus-icon-canvas-item.h \
	nautilus-icon-container.c \
	nautilus-icon-container.h \
	nautilus-icon-dnd.c \
	nautilus-icon-dnd.h \
	nautilus-icon-private.h \
	nautilus-icon-info.c \
	nautilus-icon-info.h \
	nautilus-icon-names.h \
	nautilus-lib-self-check-functions.c \
	nautilus-lib-self-check-functions.h \
	nautilus-link.c \
	nautilus-link.h \
	nautilus-merged-directory.c \
	nautilus-merged-directory.h \
	nautilus-metadata.h \
	nautilus-metadata.c \
	nautilus-mime-application-chooser.c \
	nautilus-mime-application-chooser.h \
	nautilus-module.c \
	nautilus-module.h \
	nautilus-monitor.c \
	nautilus-monitor.h \
	nautilus-progress-info.c \
	nautilus-progress-info.h \
	nautilus-progress-info-manager.c \
	nautilus-progress-info-manager.h \
	nautilus-program-choosing.c \
	nautilus-program-choosing.h \
	nautilus-recent.c \
	nautilus-recent.h \
	nautilus-saved-search-file.c \
	nautilus-saved-search-file.h \
	nautilus-search-directory.c \
	nautilus-search-directory.h \
	nautilus-search-directory-file.c \
	nautilus-search-directory-file.h \
	nautilus-search-engine.c \
	nautilus-search-engine.h \
	nautilus-search-engine-simple.c \
	nautilus-search-engine-simple.h \
	nautilus-selection-canvas-item.c \
	nautilus-selection-canvas-item.h \
	nautilus-signaller.h \
	nautilus-signaller.c \
	nautilus-query.c \
	nautilus-query.h \
	nautilus-thumbnails.c \
	nautilus-thumbnails.h \
	nautilus-trash-monitor.c \
	nautilus-trash-monitor.h \
	nautilus-tree-view-drag-dest.c \
	nautilus-tree-view-drag-dest.h \
	nautilus-ui-utilities.c \
	nautilus-ui-utilities.h \
	nautilus-undo-manager.c \
	nautilus-undo-manager.h \
	nautilus-undo-private.h \
	nautilus-undo-signal-handlers.c \
	nautilus-undo-signal-handlers.h \
	nautilus-undo-transaction.c \
	nautilus-undo-transaction.h \
	nautilus-undo.c \
	nautilus-undo.h \
	nautilus-vfs-directory.c \
	nautilus-vfs-directory.h \
	nautilus-vfs-file.c \
	nautilus-vfs-file.h \
	nautilus-file-undo-operations.c \
	nautilus-file-undo-operations.h \
	nautilus-file-undo-manager.c \
	nautilus-file-undo-manager.h \
	$(nautilus_tracker_engine_sources) \
	$(NULL)

nodist_libnautilus_private_la_SOURCES =\
	$(BUILT_SOURCES) \
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

gsettingsschema_in_files = org.gnome.nautilus.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

convertdir = $(datadir)/GConf/gsettings
convert_DATA = nautilus.convert

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

EXTRA_DIST =				\
	nautilus.convert		\
	$(gsettingsschema_in_files)     \
	$(TRACKER_SOURCES)		\
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)				\
	$(gsettings_SCHEMAS)				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
