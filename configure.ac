AC_PREREQ(2.54)

dnl ===========================================================================

m4_define(glib_minver,                 2.32.4)
m4_define(cinnamon_desktop_minver,     1.0.0)
m4_define(gnome_desktop_minver,        3.0.0)
m4_define(pango_minver,                1.28.3)
m4_define(gtk_minver,                  3.3.17)
m4_define(xml_minver,                  2.7.8)
m4_define(exif_minver,                 0.6.20)
m4_define(exempi_minver,               2.1.0)
m4_define(notify_minver,               0.7.0)
m4_define(glib_mime_ver,               2.34.0)
m4_define(schemas_minver,              3.4.0)
m4_define(unity_minver,                4.0.0)
m4_define(dbusmenu_minver,             0.4.90)
m4_define(zeitgeist_minver,            0.3.18)

dnl 1. If the library code has changed at all since last release, then increment revision.
dnl 2. If any interfaces have been added, then increment current and set revision to 0.
dnl Interface break is not allowed.
m4_define(nemo_extension_current,  5)
m4_define(nemo_extension_revision, 0)

AC_INIT(nemo, 2.5.1-daschuer5 https://github.com/daschuer/nemo)

dnl ===========================================================================

AC_CONFIG_SRCDIR(src)
AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])
GNOME_MAINTAINER_MODE_DEFINES

dnl We need to decrement current by one in the calculation of the age because
dnl the library was started with version "1:0:0" instead of "0:0:0"
AC_SUBST(NEMO_EXTENSION_VERSION_INFO, [nemo_extension_current]:[nemo_extension_revision]:`expr [nemo_extension_current] - 1`)

AC_C_BIGENDIAN
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT([disable-static])

AC_PATH_PROG(PKG_CONFIG, pkg-config, no)

AC_CHECK_LIB(m, floor)

dnl ==========================================================================

GETTEXT_PACKAGE=nemo

AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",
                   [the gettext translation domain])

AM_GLIB_GNU_GETTEXT
LOCALE_DIR=/usr/share/locale
AC_SUBST(LOCALE_DIR)

AC_DEFINE_UNQUOTED(LOCALEDIR, "$LOCALE_DIR", [path for translations])

GLIB_GSETTINGS

IT_PROG_INTLTOOL([0.40.1])

dnl ==========================================================================

GTK_DOC_CHECK([1.4])

dnl ==========================================================================

AC_ARG_ENABLE(debug,
  AS_HELP_STRING([--disable-debug],[Disable debugging code]),
  [
    case "${enableval}" in
      yes|no) enable_debug="${enableval}" ;;
      *)   AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
    esac
  ],
  [enable_debug=yes])

if test "$enable_debug" = yes; then
  AC_DEFINE(ENABLE_DEBUG, [], [Enable debug code])
else
  enable_debug=no
fi

dnl ==========================================================================
dnl Enable Profiling

AC_ARG_ENABLE(profiling,
        [AS_HELP_STRING([--enable-profiling],
                        [turn on profiling])],
        , enable_profiling=yes)

if test "x$enable_profiling" = "xyes"; then
    AC_DEFINE(ENABLE_PROFILING,1,[enable profiling])
fi


dnl ==========================================================================

AC_CHECK_PROGS(PERL, perl5 perl)
AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ==========================================================================

AC_CHECK_HEADERS(sys/mount.h sys/vfs.h sys/param.h malloc.h)
AC_CHECK_FUNCS(mallopt)

dnl ==========================================================================
dnl desktop checking

AM_CONDITIONAL(GNOME_BUILD, false)
AC_ARG_ENABLE(gnome,
	AC_HELP_STRING([--enable_gnome],
			[build against gnome libraryies]))

msg_desktop=non
cinnamon_build=yes
if test "x$enable_gnome" != "xyes"; then
	PKG_CHECK_MODULES(DESKTOP, cinnamon-desktop >= cinnamon_desktop_minver, 
			[cinnamon_build=yes] msg_desktop=Cinnamon, [cinnamon_build=no])
fi

if test "x$cinnamon_build" = "xno"; then
	PKG_CHECK_MODULES(DESKTOP, gnome-desktop-3.0 >= gnome_desktop_minver, [
			  AM_CONDITIONAL(GNOME_BUILD, true)
			  AC_DEFINE(GNOME_BUILD, 1, [Define to build against gnome libs])
			  ] msg_desktop=Gnome,
			  [AM_CONDITIONAL(GNOME_BUILD, false)])
fi

if test "x$msg_desktop" = "xnon"; then
	AC_MSG_ERROR(
		[No package 'cinnamon-desktop' >= cinnamon_desktop_minver \
or 'gnome-desktop-3.0' >= gnome_desktop_minver found])
fi

AC_SUBST(DESKTOP_CFLAGS)
AC_SUBST(DESKTOP_LIBS)

dnl ==========================================================================
dnl zeitgeist checking

AC_ARG_ENABLE(zeitgeist,
              [AS_HELP_STRING([--enable-zeitgeist=@<:@yes/no/auto@:>@],
                              [build Zeitgeist activity logging [default=auto]])],
              [enable_zeitgeist="$enableval"],
              [enable_zeitgeist=auto])

build_zeitgeist=no
if test "x$enable_zeitgeist" != "xno"; then
	PKG_CHECK_MODULES(ZEITGEIST, zeitgeist-1.0, 
                          [build_zeitgeist=yes],
                          [build_zeitgeist=no])
	if test "x$build_zeitgeist" = "xyes"; then
		AC_DEFINE(BUILD_ZEITGEIST, 1, [Define to enable Zeitgeist support])
	fi
fi

AM_CONDITIONAL(ENABLE_ZEITGEIST, test "x$build_zeitgeist" = "xyes")


dnl ==========================================================================
dnl libexif checking

AC_ARG_ENABLE(libexif,
              [AS_HELP_STRING([--enable-libexif=@<:@yes/no@:>@],
                              [build EXIF support [default=yes]])],
              [enable_exif="$enableval"],
              [enable_exif=yes])

if test "x$enable_exif" != "xno"; then
  PKG_CHECK_MODULES(EXIF, libexif >= exif_minver)
  AC_DEFINE(HAVE_EXIF, 1, [Define to enable EXIF support])
fi

AM_CONDITIONAL(HAVE_EXIF, test "x$enable_exif" = "xyes")

dnl ==========================================================================
dnl exempi checking

AC_ARG_ENABLE(xmp,
              [AS_HELP_STRING([--enable-xmp=@<:@yes/no@:>@],
                              [build XMP support [default=yes]])],
              [enable_xmp="$enableval"],
              [enable_xmp=yes])

if test "x$enable_xmp" != "xno"; then
  PKG_CHECK_MODULES(EXEMPI, exempi-2.0 >= exempi_minver)
  AC_DEFINE(HAVE_EXEMPI, 1, [Define to enable XMP support])
fi

AM_CONDITIONAL(HAVE_EXEMPI, test "x$enable_xmp" = "xyes")

PKG_CHECK_MODULES(GEN_ICON, glib-2.0 >= glib_mime_ver , gen_icon_api=yes, gen_icon_api=no)
if test x$gen_icon_api = xyes ; then
  AC_DEFINE(GENERIC_ICON_API, 1, [Defined if we meet the minimum version for g_content_type_get_generic_icon_name])
fi

dnl ==========================================================================

dnl ****************************
dnl *** Check for libselinux ***
dnl ****************************
    
SELINUX_LIBS=
msg_selinux=no
AC_CHECK_LIB(selinux, is_selinux_enabled,
   [AC_CHECK_HEADERS(selinux/selinux.h,
     [AC_SEARCH_LIBS(selinux_raw_to_trans_context, selinux, 
       [AC_DEFINE(HAVE_SELINUX, 1, [Define to 1 if libselinux is available])
        SELINUX_LIBS="-lselinux"
        msg_selinux=yes])
     ])
   ])
AC_SUBST(SELINUX_LIBS)


AC_ARG_ENABLE(empty_view,
 AS_HELP_STRING([--enable-empty-view],[Enable empty view]),
 [ENABLE_EMPTY_VIEW=1
  AC_DEFINE(ENABLE_EMPTY_VIEW, 1, [define to enable the empty view that is used for performance measurement])])

AC_SUBST(ENABLE_EMPTY_VIEW)
AM_CONDITIONAL(ENABLE_EMPTY_VIEW, test "x$ENABLE_EMPTY_VIEW" = "x1")

dnl ==========================================================================

AM_CONDITIONAL(HAVE_UNITY, false)

AC_ARG_ENABLE(unity,
    AC_HELP_STRING([--disable-unity], [build without unity launcher support]))

msg_unity=no
if test "x$enable_unity" != "xno"; then
    PKG_CHECK_MODULES(UNITY,
        [unity >= unity_minver
         dbusmenu-glib-0.4 >= dbusmenu_minver], [
            AM_CONDITIONAL(HAVE_UNITY, true)
            AC_DEFINE(HAVE_UNITY, 1, [Have Unity])
            ]
            msg_unity=yes,
            [AM_CONDITIONAL(HAVE_UNITY, false)
            ])
        AC_SUBST(UNITY_CFLAGS)
        AC_SUBST(UNITY_LIBS)
fi


dnl ==========================================================================

dnl Turn on the additional warnings last, so -Werror doesn't affect other tests.

WARNING_CFLAGS=""

AC_ARG_ENABLE(more-warnings,
AS_HELP_STRING([--enable-more-warnings],[Maximum compiler warnings]),
set_more_warnings="$enableval",[
if test -d $srcdir/.git; then
	set_more_warnings=yes
else
	set_more_warnings=no
fi
])
AC_MSG_CHECKING(for more warnings, including -Werror)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
	AC_MSG_RESULT(yes)
	WARNING_CFLAGS="\
	-Wall \
	-Wmissing-declarations -Wmissing-prototypes \
	-Wnested-externs -Wpointer-arith \
	-Wcast-align \
	-Wno-deprecated-declarations \
	-Werror"

	for option in -Wstrict-aliasing=0 -Wno-pointer-sign; do
		SAVE_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $option"
		AC_MSG_CHECKING([whether gcc understands $option])
		AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],
		                  [has_option=yes],
		                  [has_option=no])
		if test $has_option = yes; then
		   	WARNING_CFLAGS="$WARNING_CFLAGS $option"
		fi
		AC_MSG_RESULT($has_option)
		CFLAGS="$SAVE_CFLAGS"
		unset has_option
		unset SAVE_CFLAGS
	done
	unset option
else
	AC_MSG_RESULT(no)
fi

AC_SUBST(WARNING_CFLAGS)

dnl ===========================================================================
dnl Check for Tracker
dnl ===========================================================================

AC_ARG_ENABLE(tracker,
              [AS_HELP_STRING([--enable-tracker=@<:@yes/no/auto@:>@],
                              [build Tracker builtin support [default=auto]])],
              [enable_tracker="$enableval"],
              [enable_tracker=auto])

build_tracker=no
if test "x$enable_tracker" != "xno"; then
  PKG_CHECK_MODULES(TRACKER, tracker-sparql-0.16, 
                    [build_tracker=yes],
  [PKG_CHECK_MODULES(TRACKER, tracker-sparql-0.14,
                    [build_tracker=yes],
                    [PKG_CHECK_MODULES(TRACKER, tracker-sparql-0.12,
		                       [build_tracker=yes],
                                       [build_tracker=no])])])
  if test "x$build_tracker" = "xyes"; then
    AC_DEFINE(ENABLE_TRACKER, 1, [Define to enable Tracker support])
  fi
fi

AM_CONDITIONAL(ENABLE_TRACKER, test "x$build_tracker" = "xyes")

dnl ==========================================================================
	
dnl base libs
PKG_CHECK_MODULES(BASE, [
	gtk+-3.0 >= gtk_minver
	glib-2.0 >= glib_minver
])

dnl common libs (eel, nemo)
PKG_CHECK_MODULES(COMMON, [
	gail-3.0
	libxml-2.0 >= xml_minver
	x11
])

dnl additional nemo libs
PKG_CHECK_MODULES(NEMO, [
	gthread-2.0 >= glib_minver
	gmodule-no-export-2.0 >= glib_minver
	gio-2.0 >= glib_minver
	gio-unix-2.0 >= glib_minver
	gsettings-desktop-schemas >= schemas_minver
	libnotify >= notify_minver
])

dnl Multimedia keys
AC_CHECK_HEADERS([X11/XF86keysym.h])

##################################################
# Check for introspection
##################################################
GOBJECT_INTROSPECTION_CHECK([0.6.4])

dnl ==========================================================================

AC_PATH_PROG(UPDATE_MIME_DATABASE, update-mime-database, no)

AC_ARG_ENABLE(update-mimedb,
   AS_HELP_STRING([--disable-update-mimedb],
                   [disable the update-mime-database after install [default=no]]),,
    enable_update_mimedb=yes)
AM_CONDITIONAL(ENABLE_UPDATE_MIMEDB, test x$enable_update_mimedb = xyes)

CFLAGS="$CFLAGS -Werror=unused-function -Werror=unused-variable -O3"

AC_CONFIG_FILES([
Makefile
data/Makefile
data/icons/Makefile
data/nemo.desktop.in
data/nemo-autorun-software.desktop.in
data/nemo-connect-server.desktop.in
docs/Makefile
docs/reference/Makefile
docs/reference/libnemo-extension/Makefile
docs/reference/libnemo-extension/version.xml
eel/Makefile
libnemo-private/Makefile
libnemo-extension/Makefile
libnemo-extension/libnemo-extension.pc
libnemo-extension/libnemo-extension-uninstalled.pc
po/Makefile.in
src/Makefile
files/Makefile
test/Makefile
])

AC_OUTPUT

dnl ==========================================================================
echo "
nemo-$VERSION:

	prefix:                 ${prefix}
	source code location:	${srcdir}
	compiler:		${CC}
	desktop libraries:	$msg_desktop
	Unity support:		$msg_unity
	libexif support:	${enable_exif}
	libexempi support:	${enable_xmp}
	PackageKit support:     $msg_packagekit
	nautilus-sendto ext:	$enable_nst_extension
	Tracker support:	$build_tracker
	Zeitgeist logging: 	$build_zeitgeist

	debugging support:      ${enable_debug}
	profiling support:      ${enable_profiling}
	nemo-extension documentation: ${enable_gtk_doc}
	nemo-extension introspection: ${found_introspection}
"
