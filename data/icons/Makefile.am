NULL =

public_icons_themes = \
	hicolor	\
	$(NULL)

public_icons = \
	hicolor/apps/16x16/nemo.png \
	hicolor/apps/22x22/nemo.png \
	hicolor/apps/24x24/nemo.png \
	hicolor/apps/32x32/nemo.png \
	hicolor/apps/scalable/nemo.svg \
	hicolor/actions/16x16/nemo-eject.png \
    hicolor/actions/16x16/menu-bullet.png \
    hicolor/actions/16x16/menu-sort-up.png \
    hicolor/actions/16x16/menu-sort-up-free.png \
    hicolor/actions/16x16/menu-sort-down.png \
    hicolor/actions/16x16/menu-sort-down-free.png \
    hicolor/actions/16x16/menu-sort-right.png \
    hicolor/actions/16x16/menu-sort-right-free.png \
    hicolor/actions/16x16/menu-sort-left.png \
    hicolor/actions/16x16/menu-sort-left-free.png \
    hicolor/actions/16x16/menu-none.png \
	hicolor/actions/32x32/nemo-eject.png \
	hicolor/actions/scalable/view-compact-symbolic.svg \
	hicolor/actions/scalable/location-symbolic.svg \
	hicolor/actions/scalable/sidebar-hide-symbolic.svg \
	hicolor/actions/scalable/sidebar-show-symbolic.svg \
	hicolor/actions/scalable/sidebar-tree-symbolic.svg \
	hicolor/actions/scalable/sidebar-places-symbolic.svg \
	hicolor/actions/scalable/expand-menu-symbolic.svg \
	hicolor/actions/scalable/expand-menu-hover-symbolic.svg \
	hicolor/actions/scalable/collapse-menu-symbolic.svg \
	hicolor/actions/scalable/collapse-menu-hover-symbolic.svg \
	hicolor/status/48x48/progress-0.png \
	hicolor/status/48x48/progress-10.png \
	hicolor/status/48x48/progress-20.png \
	hicolor/status/48x48/progress-30.png \
	hicolor/status/48x48/progress-40.png \
	hicolor/status/48x48/progress-50.png \
	hicolor/status/48x48/progress-60.png \
	hicolor/status/48x48/progress-70.png \
	hicolor/status/48x48/progress-80.png \
	hicolor/status/48x48/progress-90.png \
	hicolor/status/48x48/progress-100.png \
	$(NULL)

private_icons = \
	hicolor/emblems/16x16/emblem-note.png \
	hicolor/emblems/24x24/emblem-note.png \
	hicolor/emblems/48x48/emblem-note.png \
	$(NULL)

noinst_DATA = \
	hicolor/apps/16x16/nemo.svg \
	hicolor/apps/22x22/nemo.svg \
	hicolor/apps/32x32/nemo.svg \
	$(NULL)

EXTRA_DIST = \
	$(public_icons)		\
	$(private_icons)	\
	$(noinst_DATA)		\
	$(NULL)


###############################################################################

gtk_update_icon_cache = gtk-update-icon-cache -f -t

update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		for theme in $(public_icons_themes); do \
			$(gtk_update_icon_cache) $(datadir)/icons/$$theme; \
		done; \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		for theme in $(public_icons_themes); do \
			echo "***   $(gtk_update_icon_cache) $(datadir)/icons/$$theme"; \
		done; \
	fi

install-icons:
	for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d/ -f1`; \
		CONTEXT=`echo $$icon | cut -d/ -f2`; \
		SIZE=`echo $$icon | cut -d/ -f3`; \
		ICONFILE=`echo $$icon | cut -d/ -f4`; \
		mkdir -p $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d/ -f1`; \
		CONTEXT=`echo $$icon | cut -d/ -f2`; \
		SIZE=`echo $$icon | cut -d/ -f3`; \
		ICONFILE=`echo $$icon | cut -d/ -f4`; \
		mkdir -p $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT; \
		$(INSTALL_DATA) $(srcdir)/$$icon $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

uninstall-icons:
	-for icon in $(public_icons); do \
		THEME=`echo $$icon | cut -d/ -f1`; \
		CONTEXT=`echo $$icon | cut -d/ -f2`; \
		SIZE=`echo $$icon | cut -d/ -f3`; \
		ICONFILE=`echo $$icon | cut -d/ -f4`; \
		rm -f $(DESTDIR)$(datadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done; \
	for icon in $(private_icons); do \
		THEME=`echo $$icon | cut -d/ -f1`; \
		CONTEXT=`echo $$icon | cut -d/ -f2`; \
		SIZE=`echo $$icon | cut -d/ -f3`; \
		ICONFILE=`echo $$icon | cut -d/ -f4`; \
		rm -f $(DESTDIR)$(pkgdatadir)/icons/$$THEME/$$SIZE/$$CONTEXT/$$ICONFILE; \
	done

install-data-local: install-icons update-icon-cache

uninstall-local: uninstall-icons update-icon-cache
