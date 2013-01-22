include $(top_srcdir)/Makefile.shared

bin_PROGRAMS=					\
	nautilus				\
	nautilus-autorun-software		\
	nautilus-connect-server			\
	$(NULL)

libexec_PROGRAMS=				\
	nautilus-convert-metadata		\
	$(NULL)

INCLUDES =							\
	-I$(top_srcdir) 					\
	-I$(top_builddir)/libnautilus-private 			\
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NAUTILUS_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	-DUIDIR=\""$(datadir)/nautilus/ui"\" 		\
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" 	\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED) 		                        \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus-private/libnautilus-private.la  \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NAUTILUS_LIBS) \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(NULL)

dbus_freedesktop_built_sources =			\
	nautilus-freedesktop-generated.c		\
	nautilus-freedesktop-generated.h

$(dbus_freedesktop_built_sources) : Makefile.am $(top_srcdir)/data/freedesktop-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.freedesktop.					\
		--c-namespace NautilusFreedesktop					\
		--c-generate-object-manager						\
		--generate-c-code nautilus-freedesktop-generated			\
		$(top_srcdir)/data/freedesktop-dbus-interfaces.xml			\
		$(NULL)

dbus_shell_search_provider_built_sources =		\
	nautilus-shell-search-provider-generated.c	\
	nautilus-shell-search-provider-generated.h

$(dbus_shell_search_provider_built_sources) : Makefile.am $(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.gnome.						\
		--c-namespace Nautilus							\
		--generate-c-code nautilus-shell-search-provider-generated		\
		$(top_srcdir)/data/shell-search-provider-dbus-interfaces.xml		\
		$(NULL)

headers =					\
	nautilus-special-location-bar.h		\
	$(NULL)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/nautilus.gresource.xml)
nautilus-resources.c: nautilus.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name nautilus $(srcdir)/nautilus.gresource.xml
nautilus-resources.h: nautilus.gresource.xml $(resource_files)
	glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name nautilus $(srcdir)/nautilus.gresource.xml

nautilus-enum-types.h: stamp-nautilus-enum-types.h
	$(AM_V_GEN)true

stamp-nautilus-enum-types.h: $(headers) Makefile
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#ifndef __NAUTILUS_ENUM_TYPES_H__\n" \
			--fhead "#define __NAUTILUS_ENUM_TYPES_H__ 1\n\n" \
			--fhead "#include <glib-object.h>\n\n" \
			--fhead "G_BEGIN_DECLS\n\n" \
			--ftail "G_END_DECLS\n\n" \
			--ftail "#endif /* __NAUTILUS_ENUM_TYPES_H__ */\n" \
			--fprod "#include \"@filename@\"\n" \
			--eprod "#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ @enum_name@_get_type()\n" \
			--eprod "GType @enum_name@_get_type (void);\n" \
		$(headers) ) > xgen-gth \
	&& (cmp -s xgen-gth nautilus-enum-type.h || cp xgen-gth nautilus-enum-types.h) \
	&& rm -f xgen-gth \
	&& echo timestamp > $(@F)

nautilus-enum-types.c: $(headers) Makefile
	$(AM_V_GEN)(cd $(srcdir) \
	&& glib-mkenums \
			--fhead "#include <config.h>\n" \
			--fhead "#include <glib-object.h>\n" \
			--fhead "#include \"nautilus-enum-types.h\"\n\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "static const G@Type@Value _@enum_name@_values[] = {" \
			--vprod "  { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "  { 0, NULL, NULL }\n};\n\n" \
			--vtail "GType\n@enum_name@_get_type (void)\n{\n" \
			--vtail "  static GType type = 0;\n\n" \
			--vtail "  if (!type)\n" \
			--vtail "    type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n\n" \
			--vtail "  return type;\n}\n\n" \
		$(headers) ) > xgen-gtc \
	&& cp xgen-gtc $(@F) \
	&& rm -f xgen-gtc

nautilus_built_sources = \
	$(dbus_freedesktop_built_sources) \
	$(dbus_shell_search_provider_built_sources) \
	nautilus-resources.c \
	nautilus-resources.h \
	nautilus-enum-types.h \
	nautilus-enum-types.c \
	$(NULL)

nautilus_SOURCES = \
	nautilus-actions.h			\
	nautilus-application.c			\
	nautilus-application.h			\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-bookmarks-window.c		\
	nautilus-bookmarks-window.h		\
	nautilus-canvas-view.c			\
	nautilus-canvas-view.h			\
	nautilus-canvas-view-container.c	\
	nautilus-canvas-view-container.h	\
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-desktop-canvas-view.c		\
	nautilus-desktop-canvas-view.h		\
	nautilus-desktop-item-properties.c	\
	nautilus-desktop-item-properties.h	\
	nautilus-desktop-window.c		\
	nautilus-desktop-window.h		\
	nautilus-error-reporting.c		\
	nautilus-error-reporting.h		\
	nautilus-file-management-properties.c	\
	nautilus-file-management-properties.h	\
	nautilus-floating-bar.c			\
	nautilus-floating-bar.h			\
	nautilus-freedesktop-dbus.c		\
	nautilus-freedesktop-dbus.h		\
	nautilus-image-properties-page.c	\
	nautilus-image-properties-page.h	\
	nautilus-list-model.c			\
	nautilus-list-model.h			\
	nautilus-list-view-private.h 		\
	nautilus-list-view.c			\
	nautilus-list-view.h 			\
	nautilus-location-entry.c               \
	nautilus-location-entry.h               \
	nautilus-main.c 			\
	nautilus-mime-actions.c 		\
	nautilus-mime-actions.h 		\
	nautilus-notebook.c			\
	nautilus-notebook.h			\
	nautilus-pathbar.c			\
	nautilus-pathbar.h			\
	nautilus-places-sidebar.c		\
	nautilus-places-sidebar.h		\
	nautilus-previewer.c			\
	nautilus-previewer.h			\
	nautilus-progress-info-widget.c		\
	nautilus-progress-info-widget.h		\
	nautilus-progress-ui-handler.c		\
	nautilus-progress-ui-handler.h		\
	nautilus-properties-window.c		\
	nautilus-properties-window.h		\
	nautilus-query-editor.c			\
	nautilus-query-editor.h			\
	nautilus-self-check-functions.c 	\
	nautilus-self-check-functions.h 	\
	nautilus-shell-search-provider.h	\
	nautilus-shell-search-provider.c	\
	nautilus-special-location-bar.c		\
	nautilus-special-location-bar.h		\
	nautilus-toolbar.c			\
	nautilus-toolbar.h			\
	nautilus-trash-bar.c			\
	nautilus-trash-bar.h			\
	nautilus-view.c				\
	nautilus-view.h				\
	nautilus-view-dnd.c			\
	nautilus-view-dnd.h			\
	nautilus-view-factory.c 		\
	nautilus-view-factory.h 		\
	nautilus-window-menus.c         	\
	nautilus-window-private.h		\
	nautilus-window-slot.c			\
	nautilus-window-slot.h			\
	nautilus-window-slot-dnd.c		\
	nautilus-window-slot-dnd.h		\
	nautilus-window.c			\
	nautilus-window.h			\
	nautilus-x-content-bar.c		\
	nautilus-x-content-bar.h		\
	$(NULL)

nodist_nautilus_SOURCES = \
	$(nautilus_built_sources) \
	$(NULL)

EMPTY_VIEW_SOURCES = \
	nautilus-empty-view.c 			\
	nautilus-empty-view.h

if ENABLE_EMPTY_VIEW
nautilus_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

nautilus_autorun_software_SOURCES= 			\
	nautilus-autorun-software.c			\
	$(NULL)

nautilus_connect_server_SOURCES= \
	nautilus-connect-server-dialog.c        \
	nautilus-connect-server-dialog.h        \
	nautilus-connect-server.c		\
	$(NULL)

nautilus_convert_metadata_SOURCES= \
	nautilus-convert-metadata.c	\
	$(NULL)

BUILT_SOURCES = 					\
	$(nautilus_built_sources) 			\
	$(dbus_shell_search_provider_built_sources) 	\
	$(NULL)

TESTS=check-nautilus

@INTLTOOL_SERVER_RULE@

CLEANFILES = \
	$(BUILT_SOURCES)			\
	$(server_DATA)				\
	stamp-nautilus-enum-types.h		\
	$(NULL)

EXTRA_DIST =					\
	$(resource_files)			\
	$(server_in_files)			\
	nautilus.gresource.xml			\
	check-nautilus				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
