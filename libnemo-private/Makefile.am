include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnemo-private.la

INCLUDES = 						\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_srcdir)/cut-n-paste-code		\
	$(BASE_CFLAGS)					\
	$(COMMON_CFLAGS)				\
	$(NEMO_CFLAGS)				\
	$(WARNING_CFLAGS)				\
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(TRACKER_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\" 			\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DNEMO_DATADIR=\""$(datadir)/nemo"\" 	\
	-DNEMO_EXTENSIONDIR=\""$(libdir)/nemo/extensions-3.0"\" \
	$(NULL)

dbus_built_sources = nemo-generated.c nemo-generated.h
$(dbus_built_sources) : Makefile.am $(top_srcdir)/data/dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.Nemo.					\
		--c-namespace NemoDBus						\
		--c-generate-object-manager						\
		--generate-c-code nemo-generated					\
		$(top_srcdir)/data/dbus-interfaces.xml					\
		$(NULL)

BUILT_SOURCES = \
	$(dbus_built_sources) \
	$(NULL)

if ENABLE_TRACKER
nemo_tracker_engine_sources = \
	nemo-search-engine-tracker.c \
	nemo-search-engine-tracker.h
endif

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/libegg/libegg.la \
	$(NULL)

libnemo_private_la_LDFLAGS =	\
	-no-undefined			\
	$(NULL)

libnemo_private_la_LIBADD =		\
	$(dependency_static_libs)	\
	$(SELINUX_LIBS)			\
	$(TRACKER_LIBS)			\
	$(top_builddir)/eel/libeel-2.la \
	$(top_builddir)/libnemo-extension/libnemo-extension.la \
	$(BASE_LIBS)			\
	$(COMMON_LIBS)			\
	$(NEMO_LIBS)		\
	-lm				\
	$(NULL)

libnemo_private_la_SOURCES = \
    nemo-action.c \
    nemo-action.h \
    nemo-action-manager.c \
    nemo-action-manager.h \
	nemo-bookmark.c \
	nemo-bookmark.h \
	nemo-canvas-container.c \
	nemo-canvas-container.h \
	nemo-canvas-dnd.c \
	nemo-canvas-dnd.h \
	nemo-canvas-item.c \
	nemo-canvas-item.h \
	nemo-canvas-private.h \
    nemo-cell-renderer-disk.c \
    nemo-cell-renderer-disk.h \
	nemo-clipboard-monitor.c \
	nemo-clipboard-monitor.h \
	nemo-clipboard.c \
	nemo-clipboard.h \
	nemo-column-chooser.c \
	nemo-column-chooser.h \
	nemo-column-utilities.c \
	nemo-column-utilities.h \
	nemo-dbus-manager.c \
	nemo-dbus-manager.h \
	nemo-debug.c \
	nemo-debug.h \
	nemo-default-file-icon.c \
	nemo-default-file-icon.h \
	nemo-desktop-directory-file.c \
	nemo-desktop-directory-file.h \
	nemo-desktop-directory.c \
	nemo-desktop-directory.h \
	nemo-desktop-icon-file.c \
	nemo-desktop-icon-file.h \
	nemo-desktop-link-monitor.c \
	nemo-desktop-link-monitor.h \
	nemo-desktop-link.c \
	nemo-desktop-link.h \
	nemo-desktop-metadata.c \
	nemo-desktop-metadata.h \
	nemo-directory-async.c \
	nemo-directory-notify.h \
	nemo-directory-private.h \
	nemo-directory.c \
	nemo-directory.h \
	nemo-dnd.c \
	nemo-dnd.h \
	nemo-entry.c \
	nemo-entry.h \
	nemo-file-attributes.h \
	nemo-file-changes-queue.c \
	nemo-file-changes-queue.h \
	nemo-file-conflict-dialog.c \
	nemo-file-conflict-dialog.h \
	nemo-file-dnd.c \
	nemo-file-dnd.h \
	nemo-file-operations.c \
	nemo-file-operations.h \
	nemo-file-private.h \
	nemo-file-queue.c \
	nemo-file-queue.h \
	nemo-file-utilities.c \
	nemo-file-utilities.h \
	nemo-file.c \
	nemo-file.h \
	nemo-generated.c \
	nemo-generated.h \
	nemo-global-preferences.c \
	nemo-global-preferences.h \
	nemo-icon-info.c \
	nemo-icon-info.h \
	nemo-icon-names.h \
	nemo-lib-self-check-functions.c \
	nemo-lib-self-check-functions.h \
	nemo-link.c \
	nemo-link.h \
	nemo-merged-directory.c \
	nemo-merged-directory.h \
	nemo-metadata.h \
	nemo-metadata.c \
	nemo-mime-application-chooser.c \
	nemo-mime-application-chooser.h \
	nemo-module.c \
	nemo-module.h \
	nemo-monitor.c \
	nemo-monitor.h \
    nemo-pathbar-button.c \
    nemo-pathbar-button.h \
    nemo-places-tree-view.c \
    nemo-places-tree-view.h \
	nemo-progress-info.c \
	nemo-progress-info.h \
	nemo-progress-info-manager.c \
	nemo-progress-info-manager.h \
	nemo-program-choosing.c \
	nemo-program-choosing.h \
	nemo-recent.c \
	nemo-recent.h \
	nemo-saved-search-file.c \
	nemo-saved-search-file.h \
	nemo-search-directory.c \
	nemo-search-directory.h \
	nemo-search-directory-file.c \
	nemo-search-directory-file.h \
	nemo-search-provider.c \
	nemo-search-provider.h \
	nemo-search-engine.c \
	nemo-search-engine.h \
	nemo-search-engine-simple.c \
	nemo-search-engine-simple.h \
	nemo-search-hit.c \
	nemo-search-hit.h \
	nemo-selection-canvas-item.c \
	nemo-selection-canvas-item.h \
	nemo-signaller.h \
	nemo-signaller.c \
	nemo-query.c \
	nemo-query.h \
	nemo-thumbnails.c \
	nemo-thumbnails.h \
	nemo-trash-monitor.c \
	nemo-trash-monitor.h \
	nemo-tree-view-drag-dest.c \
	nemo-tree-view-drag-dest.h \
	nemo-ui-utilities.c \
	nemo-ui-utilities.h \
	nemo-vfs-directory.c \
	nemo-vfs-directory.h \
	nemo-vfs-file.c \
	nemo-vfs-file.h \
	nemo-file-undo-operations.c \
	nemo-file-undo-operations.h \
	nemo-file-undo-manager.c \
	nemo-file-undo-manager.h \
	$(nemo_tracker_engine_sources) \
	$(NULL)

nodist_libnemo_private_la_SOURCES =\
	$(BUILT_SOURCES) \
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

gsettingsschema_in_files = org.nemo.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

EXTRA_DIST =				\
	$(gsettingsschema_in_files)     \
	$(TRACKER_SOURCES)		\
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES)				\
	$(gsettings_SCHEMAS)				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
