NULL=

xml_in_files = nemo.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	org.Nemo.desktop.in				\
	nemo-autorun-software.desktop.in		\
	nemo-connect-server.desktop.in

autostartdir     = $(datadir)/applications
autostart_in_files = nemo-autostart.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

appdatadir = $(datadir)/appdata
appdata_in_files = org.Nemo.appdata.xml.in
appdata_DATA = $(appdata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.nemo.freedesktop.FileManager1.service.in	\
	org.Nemo.service.in

org.Nemo.service: org.Nemo.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

org.nemo.freedesktop.FileManager1.service: org.nemo.freedesktop.FileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

searchproviderdir = $(datadir)/gnome-shell/search-providers
searchprovider_DATA = nemo-search-provider.ini

nemodatadir = $(datadir)/nemo

EXTRA_DIST = $(nemodata_DATA) 	\
	org.Nemo.desktop		\
	org.Nemo.desktop.in		\
	dbus-interfaces.xml		\
	freedesktop-dbus-interfaces.xml	\
	shell-search-provider-dbus-interfaces.xml \
	$(xml_in_files) 		\
	$(desktop_in_files)		\
	$(service_in_files)		\
	$(autostart_in_files)		\
	$(searchprovider_DATA)		\
	$(appdata_in_files)		\
	$(NULL)

CLEANFILES = $(xml_files)	\
	$(desktop_DATA)		\
	$(service_DATA)		\
	$(autostart_DATA)	\
	$(appdata_DATA)		\
	$(NULL)

SUBDIRS = \
	icons    \
	$(NULL)

install-data-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

uninstall-hook:
if ENABLE_UPDATE_MIMEDB
	$(UPDATE_MIME_DATABASE) "$(DESTDIR)$(datadir)/mime"
endif

-include $(top_srcdir)/git.mk
