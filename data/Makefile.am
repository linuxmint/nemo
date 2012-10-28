NULL=

xml_in_files = nemo.xml.in
xml_files = $(xml_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nemo.desktop.in				\
	nemo-autorun-software.desktop.in

autostartdir     = $(sysconfdir)/xdg/autostart
autostart_in_files = nemo-autostart.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@

mimedir = $(datadir)/mime/packages
mime_DATA = $(xml_files)

cssdir = $(datadir)/nemo
css_DATA = \
	nemo.css \
	$(NULL)

servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)
service_in_files = 				\
	org.freedesktop.NemoFileManager1.service.in	\
	org.Nemo.service.in

org.Nemo.service: org.Nemo.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

org.freedesktop.NemoFileManager1.service: org.freedesktop.NemoFileManager1.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

nemodatadir = $(datadir)/nemo

nemodata_DATA = \
	nemo-extras.placeholder 	\
	nemo-suggested.placeholder 	\
	$(NULL)

EXTRA_DIST = $(nemodata_DATA) 	\
	nemo.desktop		\
	nemo.desktop.in		\
	dbus-interfaces.xml		\
	freedesktop-dbus-interfaces.xml	\
	$(xml_in_files) 		\
	$(desktop_in_files)		\
	$(service_in_files)		\
	$(autostart_in_files)		\
	$(css_DATA)		\
	$(NULL)

CLEANFILES = $(xml_files)	\
	$(desktop_DATA)		\
	$(service_DATA)		\
	$(autostart_DATA)	\
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
