AC_PREREQ([2.60])

AC_INIT([gnome-settings-daemon],
        [3.6.4],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-settings-daemon])

AC_CONFIG_SRCDIR([gnome-settings-daemon/gnome-settings-manager.c])

AM_INIT_AUTOMAKE([1.9 tar-ustar dist-xz no-dist-gzip check-news])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

m4_define([gsd_api_version_major],[3])
m4_define([gsd_api_version_minor],[0])
m4_define([gsd_api_version],[gsd_api_version_major.gsd_api_version_minor])
GSD_API_VERSION="gsd_api_version"
AC_SUBST(GSD_API_VERSION)

AC_STDC_HEADERS
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_HEADER_STDC

AC_SUBST(VERSION)

AC_CONFIG_HEADERS([config.h])

IT_PROG_INTLTOOL([0.37.1])

GETTEXT_PACKAGE=gnome-settings-daemon
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of default gettext domain])

AM_GLIB_GNU_GETTEXT

GSD_INTLTOOL_PLUGIN_RULE='%.gnome-settings-plugin:   %.gnome-settings-plugin.in   $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< [$]@'
AC_SUBST([GSD_INTLTOOL_PLUGIN_RULE])

dnl ---------------------------------------------------------------------------
dnl - Dependencies
dnl ---------------------------------------------------------------------------

GLIB_REQUIRED_VERSION=2.31.0
GTK_REQUIRED_VERSION=3.3.18
GCONF_REQUIRED_VERSION=2.6.1
GIO_REQUIRED_VERSION=2.26.0
GNOME_DESKTOP_REQUIRED_VERSION=3.5.3
LIBNOTIFY_REQUIRED_VERSION=0.7.3
UPOWER_GLIB_REQUIRED_VERSION=0.9.1
PA_REQUIRED_VERSION=0.9.16
LIBWACOM_REQUIRED_VERSION=0.6
UPOWER_REQUIRED_VERSION=0.9.11
IBUS_REQUIRED_VERSION=1.4.99

EXTRA_COMPILE_WARNINGS(yes)

PKG_CHECK_MODULES(SETTINGS_DAEMON,
        glib-2.0 >= $GLIB_REQUIRED_VERSION
        gtk+-3.0 >= $GTK_REQUIRED_VERSION
        gio-2.0 >= $GIO_REQUIRED_VERSION
        gmodule-2.0
        gthread-2.0
	gsettings-desktop-schemas >= 3.5.90
)

PKG_CHECK_MODULES(SETTINGS_PLUGIN,
        gtk+-3.0 >= $GTK_REQUIRED_VERSION
        gio-2.0 >= $GIO_REQUIRED_VERSION
        libnotify >= $LIBNOTIFY_REQUIRED_VERSION
        gsettings-desktop-schemas
        x11
)

GSD_PLUGIN_LDFLAGS="-export_dynamic -module -avoid-version -no-undefined"
case $host_os in
  darwin*)
    GSD_PLUGIN_LDFLAGS="${GSD_PLUGIN_LDFLAGS} -Wl,-bundle_loader,\$(top_builddir)/gnome-settings-daemon/gnome-settings-daemon"
    ;;
esac
AC_SUBST([GSD_PLUGIN_LDFLAGS])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ================================================================
dnl GSettings stuff
dnl ================================================================

GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check for gnome-desktop
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GNOME_DESKTOP, gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION)

dnl ---------------------------------------------------------------------------
dnl - Check for LCMS2
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(LCMS, lcms2 >= 2.2, have_new_lcms=yes, have_new_lcms=no)
if test x$have_new_lcms = xyes; then
        AC_DEFINE(HAVE_NEW_LCMS,1,[Got new lcms2])
else
        PKG_CHECK_MODULES(LCMS, lcms2)
fi

dnl ---------------------------------------------------------------------------
dnl - Check for libnotify
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED_VERSION,
		  [have_libnotify=yes], have_libnotify=no)
if test "x$have_libnotify" = xno ; then
	AC_MSG_ERROR([libnotify is required to build gnome-settings-daemon])
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

dnl ---------------------------------------------------------------------------
dnl - GUdev integration (default enabled)
dnl ---------------------------------------------------------------------------
GUDEV_PKG=""
AC_ARG_ENABLE(gudev, AS_HELP_STRING([--disable-gudev],[Disable GUdev support (not optional on Linux platforms)]), enable_gudev=$enableval)
if test x$enable_gudev != xno; then
	PKG_CHECK_MODULES(GUDEV, gudev-1.0, have_gudev="yes", have_gudev="no")
	if test "x$have_gudev" = "xyes"; then
		AC_DEFINE(HAVE_GUDEV, 1, [define if GUdev is available])
		GUDEV_PKG="gudev-1.0"
	else
		if test x$enable_gudev = xyes; then
			AC_MSG_ERROR([GUdev enabled but not found])
		fi
	fi
else
	have_gudev=no
fi
AM_CONDITIONAL(HAVE_GUDEV, test x$have_gudev = xyes)

dnl ---------------------------------------------------------------------------
dnl - common
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(COMMON, x11 kbproto xi)

dnl ---------------------------------------------------------------------------
dnl - automount
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(AUTOMOUNT, x11 kbproto)

dnl ---------------------------------------------------------------------------
dnl - background
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(BACKGROUND, x11 gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION)

dnl ---------------------------------------------------------------------------
dnl - mouse
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(MOUSE, x11 xi)

dnl ---------------------------------------------------------------------------
dnl - cursor
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(CURSOR, xfixes)

dnl ---------------------------------------------------------------------------
dnl - xsettings
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(XSETTINGS, fontconfig)

dnl ---------------------------------------------------------------------------
dnl - Keyboard plugin stuff
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(ibus,
        AS_HELP_STRING([--disable-ibus],
                       [Disable IBus support]),
        enable_ibus=$enableval,
        enable_ibus=yes)

if test "x$enable_ibus" = "xyes" ; then
        IBUS_MODULE="ibus-1.0 >= $IBUS_REQUIRED_VERSION"
        AC_DEFINE(HAVE_IBUS, 1, [Defined if IBus support is enabled])
else
        IBUS_MODULE=
fi
AM_CONDITIONAL(HAVE_IBUS, test "x$enable_ibus" == "xyes")

PKG_CHECK_MODULES(KEYBOARD, xkbfile $IBUS_MODULE gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION)

dnl ---------------------------------------------------------------------------
dnl - Housekeeping plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(GIOUNIX, [gio-unix-2.0])

dnl ---------------------------------------------------------------------------
dnl - media-keys plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(MEDIA_KEYS, [gio-unix-2.0 libpulse >= $PA_REQUIRED_VERSION $GUDEV_PKG libpulse-mainloop-glib >= $PA_REQUIRED_VERSION libcanberra])

dnl ---------------------------------------------------------------------------
dnl - xrandr plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(XRANDR, [gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION upower-glib >= $UPOWER_REQUIRED_VERSION])

dnl ---------------------------------------------------------------------------
dnl - orientation plugin stuff
dnl ---------------------------------------------------------------------------

if test x$have_gudev != xno; then
	PKG_CHECK_MODULES(ORIENTATION, [gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION gudev-1.0])
fi

dnl ---------------------------------------------------------------------------
dnl - sound plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(SOUND, [libpulse >= $PA_REQUIRED_VERSION $GUDEV_PKG libpulse-mainloop-glib >= $PA_REQUIRED_VERSION])

# ---------------------------------------------------------------------------
# Power
# ---------------------------------------------------------------------------
PKG_CHECK_MODULES(POWER, upower-glib >= $UPOWER_REQUIRED_VERSION gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION libcanberra-gtk3 libnotify x11 xext)

if test x$have_gudev != xno; then
	PKG_CHECK_MODULES(BACKLIGHT_HELPER,
		glib-2.0 >= $GLIB_REQUIRED_VERSION
		gudev-1.0
	)
fi

dnl ---------------------------------------------------------------------------
dnl - color
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(COLOR, [colord >= 0.1.9 gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION libcanberra-gtk3])

dnl ---------------------------------------------------------------------------
dnl - wacom (disabled for s390/s390x and non Linux platforms)
dnl ---------------------------------------------------------------------------

case $host_os in
  linux*)
    if test "$host_cpu" = s390 -o "$host_cpu" = s390x; then
      have_wacom=no
    else
      if test x$enable_gudev != xno; then
        PKG_CHECK_MODULES(WACOM, [libwacom >= $LIBWACOM_REQUIRED_VERSION x11 xi xtst gudev-1.0 gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION xorg-wacom])
      else
        AC_MSG_ERROR([GUdev is necessary to compile Wacom support])
      fi
      have_wacom=yes
    fi
    ;;
  *)
    have_wacom=no
    ;;
esac
AM_CONDITIONAL(HAVE_WACOM, test x$have_wacom = xyes)

dnl ==============================================
dnl PackageKit section
dnl ==============================================

have_packagekit=false
AC_ARG_ENABLE(packagekit,
  AC_HELP_STRING([--disable-packagekit],
                 [turn off PackageKit support]),
       [case "${enableval}" in
               yes) WANT_PACKAGEKIT=yes ;;
               no)  WANT_PACKAGEKIT=no ;;
               *) AC_MSG_ERROR(bad value ${enableval} for --disable-packagekit) ;;
       esac],
       [WANT_PACKAGEKIT=yes]) dnl Default value

if test x$WANT_PACKAGEKIT = xyes ; then
       PK_REQUIRED_VERSION=0.7.4
       PKG_CHECK_MODULES(PACKAGEKIT, glib-2.0 packagekit-glib2 >= $PK_REQUIRED_VERSION upower-glib >= $UPOWER_REQUIRED_VERSION gudev-1.0 libnotify >= $LIBNOTIFY_REQUIRED_VERSION,
             [have_packagekit=true
              AC_DEFINE(HAVE_PACKAGEKIT, 1, [Define if PackageKit should be used])],
             [have_packagekit=false])
fi
AM_CONDITIONAL(HAVE_PACKAGEKIT, test "x$have_packagekit" = "xtrue")

AC_SUBST(PACKAGEKIT_CFLAGS)
AC_SUBST(PACKAGEKIT_LIBS)

dnl ==============================================
dnl smartcard section
dnl ==============================================
have_smartcard_support=false
AC_ARG_ENABLE(smartcard-support,
  AC_HELP_STRING([--disable-smartcard-support],
                 [turn off smartcard support]),
       [case "${enableval}" in
               yes) WANT_SMARTCARD_SUPPORT=yes ;;
               no)  WANT_SMARTCARD_SUPPORT=no ;;
               *) AC_MSG_ERROR(bad value ${enableval} for --disable-smartcard-support) ;;
       esac],
       [WANT_SMARTCARD_SUPPORT=yes])

if test x$WANT_SMARTCARD_SUPPORT = xyes ; then
       NSS_REQUIRED_VERSION=3.11.2
       PKG_CHECK_MODULES(NSS, nss >= $NSS_REQUIRED_VERSION,
             [have_smartcard_support=true
              AC_DEFINE(SMARTCARD_SUPPORT, 1, [Define if smartcard support should be enabled])],
             [have_smartcard_support=false])
fi
AM_CONDITIONAL(SMARTCARD_SUPPORT, test "x$have_smartcard_support" = "xtrue")

AC_SUBST(NSS_CFLAGS)
AC_SUBST(NSS_LIBS)

AC_ARG_WITH(nssdb,
  AC_HELP_STRING([--with-nssdb],
                 [where system NSS database is]))

NSS_DATABASE=""
if test "x$have_smartcard_support" = "xtrue"; then
        if ! test -z "$with_nssdb" ; then
                NSS_DATABASE="$with_nssdb"
        else
                NSS_DATABASE="${sysconfdir}/pki/nssdb"
        fi
else
        if ! test -z "$with_nssdb" ; then
                AC_MSG_WARN([nssdb specified when smartcard support is disabled])
        fi
fi

AC_SUBST(NSS_DATABASE)


dnl ==============================================
dnl systemd check
dnl ==============================================

AC_ARG_ENABLE([systemd],
              AS_HELP_STRING([--enable-systemd], [Use systemd for session tracking]),
              [with_systemd=$enableval],
              [with_systemd=no])
if test "$with_systemd" = "yes" ; then
  PKG_CHECK_MODULES(SYSTEMD, [libsystemd-login])
  AC_DEFINE(HAVE_SYSTEMD, 1, [Define if systemd is used for session tracking])
  SESSION_TRACKING=systemd
else
  SESSION_TRACKING=ConsoleKit
fi

AC_SUBST(SYSTEMD_CFLAGS)
AC_SUBST(SYSTEMD_LIBS)

AM_CONDITIONAL(WITH_SYSTEMD, [test "$with_systemd" = "yes"], [Using systemd])

# ---------------------------------------------------------------------------
# CUPS
# ---------------------------------------------------------------------------

AC_ARG_ENABLE(cups,
              AS_HELP_STRING([--disable-cups], [disable CUPS support (default: enabled)]),,
              enable_cups=yes)

if test x"$enable_cups" != x"no" ; then
   AC_PROG_SED

   AC_PATH_PROG(CUPS_CONFIG, cups-config)

   if test x$CUPS_CONFIG = x; then
     AC_MSG_ERROR([cups-config not found but CUPS support requested])
   fi

  CUPS_API_VERSION=`$CUPS_CONFIG --api-version`
  CUPS_API_MAJOR=`echo $ECHO_N $CUPS_API_VERSION | cut -d . -f 1`
  CUPS_API_MINOR=`echo $ECHO_N $CUPS_API_VERSION | cut -d . -f 2`

  AC_CHECK_HEADERS([cups/cups.h cups/http.h cups/ipp.h],,
                   AC_MSG_ERROR([CUPS headers not found but CUPS support requested]))

   if ! test $CUPS_API_MAJOR -gt 1 -o \
             $CUPS_API_MAJOR -eq 1 -a $CUPS_API_MINOR -ge 4 ; then
      AC_MSG_ERROR([CUPS 1.4 or newer not found, but CUPS support requested])
   fi

   CUPS_CFLAGS=`$CUPS_CONFIG --cflags | $SED -e 's/-O\w*//g' -e 's/-m\w*//g'`
   CUPS_LIBS=`$CUPS_CONFIG --libs`
   AC_SUBST(CUPS_CFLAGS)
   AC_SUBST(CUPS_LIBS)
fi

AM_CONDITIONAL(BUILD_PRINT_NOTIFICATIONS, [test x"$enable_cups" = x"yes"])

# ---------------------------------------------------------------------------
# Enable Profiling
# ---------------------------------------------------------------------------
AC_ARG_ENABLE(profiling,
	[AC_HELP_STRING([--enable-profiling],
	[turn on profiling])],
	, enable_profiling=no)
if test "x$enable_profiling" = "xyes"; then
    AC_DEFINE(ENABLE_PROFILING,1,[enable profiling])
fi


# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------

plugindir='$(libdir)/gnome-settings-daemon-gsd_api_version'
AC_SUBST([plugindir])

PLUGIN_CFLAGS="-DG_LOG_DOMAIN=\"\\\"\$(plugin_name)-plugin\\\"\" -DPLUGIN_NAME=\"\\\"\$(plugin_name)\\\"\" "
AC_SUBST(PLUGIN_CFLAGS)

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=yes]])],,
              enable_man=yes)
if test "$enable_man" != no; then
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  if test -z "$XSLTPROC"; then
    AC_MSG_ERROR([xsltproc is required for --enable-man])
  fi
fi
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

dnl ---------------------------------------------------------------------------
dnl - Finish
dnl ---------------------------------------------------------------------------


# Turn on the additional warnings last, so warnings don't affect other tests.

AC_ARG_ENABLE(more-warnings,
	[AC_HELP_STRING([--enable-more-warnings],
	[Maximum compiler warnings])],
	set_more_warnings="$enableval",[
		if test -d $srcdir/.git; then
			set_more_warnings=yes
		else
			set_more_warnings=no
		fi
        ])
AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
        AC_MSG_RESULT(yes)
        CFLAGS="\
        -Wall \
        -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes \
        -Wnested-externs -Wpointer-arith \
        -Wcast-align -Wsign-compare \
        $CFLAGS"

        for option in -Wno-strict-aliasing -Wno-sign-compare; do
                SAVE_CFLAGS="$CFLAGS"
                CFLAGS="$CFLAGS $option"
                AC_MSG_CHECKING([whether gcc understands $option])
                AC_TRY_COMPILE([], [],
                        has_option=yes,
                        has_option=no,)
                if test $has_option = no; then
                        CFLAGS="$SAVE_CFLAGS"
                fi
                AC_MSG_RESULT($has_option)
                unset has_option
                unset SAVE_CFLAGS
        done
        unset option
else
        AC_MSG_RESULT(no)
fi

#
# Enable Debug
#
AC_ARG_ENABLE(debug,
	[AC_HELP_STRING([--enable-debug],
	[turn on debugging])],
	, enable_debug=yes)
if test "$enable_debug" = "yes"; then
	DEBUG_CFLAGS="-DG_ENABLE_DEBUG"
else
	if test "x$enable_debug" = "xno"; then
		DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS"
	else
		DEBUG_CFLAGS=""
	fi
fi
AC_SUBST(DEBUG_CFLAGS)

AC_OUTPUT([
Makefile
gnome-settings-daemon/Makefile
plugins/Makefile
plugins/a11y-keyboard/Makefile
plugins/a11y-settings/Makefile
plugins/automount/Makefile
plugins/background/Makefile
plugins/clipboard/Makefile
plugins/color/Makefile
plugins/common/Makefile
plugins/cursor/Makefile
plugins/dummy/Makefile
plugins/power/Makefile
plugins/housekeeping/Makefile
plugins/keyboard/Makefile
plugins/media-keys/Makefile
plugins/media-keys/cut-n-paste/Makefile
plugins/mouse/Makefile
plugins/orientation/Makefile
plugins/print-notifications/Makefile
plugins/screensaver-proxy/Makefile
plugins/smartcard/Makefile
plugins/sound/Makefile
plugins/updates/Makefile
plugins/wacom/Makefile
plugins/xrandr/Makefile
plugins/xsettings/Makefile
data/Makefile
data/gnome-settings-daemon.pc
data/gnome-settings-daemon-uninstalled.pc
data/org.gnome.settings-daemon.plugins.gschema.xml.in
data/org.gnome.settings-daemon.plugins.xsettings.gschema.xml.in
data/org.gnome.settings-daemon.plugins.keyboard.gschema.xml.in
data/org.gnome.settings-daemon.plugins.power.gschema.xml.in
data/org.gnome.settings-daemon.plugins.color.gschema.xml.in
data/org.gnome.settings-daemon.plugins.media-keys.gschema.xml.in
data/org.gnome.settings-daemon.peripherals.gschema.xml.in
data/org.gnome.settings-daemon.plugins.housekeeping.gschema.xml.in
data/org.gnome.settings-daemon.plugins.orientation.gschema.xml.in
data/org.gnome.settings-daemon.plugins.updates.gschema.xml.in
data/org.gnome.settings-daemon.plugins.xrandr.gschema.xml.in
data/org.gnome.settings-daemon.peripherals.wacom.gschema.xml.in
data/org.gnome.settings-daemon.plugins.print-notifications.gschema.xml.in
po/Makefile.in
man/Makefile
])

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
                    gnome-settings-daemon $VERSION
                    =============================

        prefix:                   ${prefix}
        exec_prefix:              ${exec_prefix}
        libdir:                   ${libdir}
        bindir:                   ${bindir}
        sbindir:                  ${sbindir}
        sysconfdir:               ${sysconfdir}
        sysconfsubdir:            ${sysconfsubdir}
        localstatedir:            ${localstatedir}
        plugindir:                ${plugindir}
        datadir:                  ${datadir}
        source code location:     ${srcdir}
        compiler:                 ${CC}
        cflags:                   ${CFLAGS}
        Maintainer mode:          ${USE_MAINTAINER_MODE}

        Session tracking:         ${SESSION_TRACKING}
        LCMS DICT support:        ${have_new_lcms}
        IBus support:             ${enable_ibus}
        Libnotify support:        ${have_libnotify}
        PackageKit support:       ${have_packagekit}
        Smartcard support:        ${have_smartcard_support}
        Cups support:             ${enable_cups}
        Wacom support:            ${have_wacom}
${NSS_DATABASE:+\
        System nssdb:             ${NSS_DATABASE}
}\
        Profiling support:        ${enable_profiling}
"
