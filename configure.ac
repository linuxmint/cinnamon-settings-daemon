AC_PREREQ([2.60])

AC_INIT([cinnamon-settings-daemon],
        [4.6.1],
        [https://github.com/linuxmint/cinnamon-settings-daemon/issues])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([cinnamon-settings-daemon/cinnamon-settings-session.c])

AM_INIT_AUTOMAKE([1.9 tar-ustar dist-xz no-dist-gzip check-news])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AX_IS_RELEASE], [AX_IS_RELEASE([always])])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

m4_define([gsd_api_version_major],[3])
m4_define([gsd_api_version_minor],[0])
m4_define([gsd_api_version],[gsd_api_version_major.gsd_api_version_minor])
CSD_API_VERSION="gsd_api_version"
AC_SUBST(CSD_API_VERSION)

AC_STDC_HEADERS
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LIBTOOL

AC_HEADER_STDC

AC_SUBST(VERSION)

AC_CONFIG_HEADERS([config.h])

IT_PROG_INTLTOOL([0.37.1])

GETTEXT_PACKAGE=cinnamon-settings-daemon
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Name of default gettext domain])

AM_GLIB_GNU_GETTEXT

m4_ifdef([AX_COMPILER_FLAGS],
         [AX_COMPILER_FLAGS([WARN_CFLAGS],[WARN_LDFLAGS])])

dnl ---------------------------------------------------------------------------
dnl - Dependencies
dnl ---------------------------------------------------------------------------

GLIB_REQUIRED_VERSION=2.37.3
GTK_REQUIRED_VERSION=3.9.10
GCONF_REQUIRED_VERSION=2.6.1
GIO_REQUIRED_VERSION=2.26.0
CINNAMON_DESKTOP_REQUIRED_VERSION=3.2.2
LIBNOTIFY_REQUIRED_VERSION=0.7.3
UPOWER_GLIB_REQUIRED_VERSION=0.9.11
PA_REQUIRED_VERSION=0.9.16
LIBWACOM_REQUIRED_VERSION=0.7
LIBRSVG_REQUIRED_VERSION=2.36.2
GTK_XINPUT_2_3_VERSION=3.7.8

#EXTRA_COMPILE_WARNINGS(yes)

PKG_CHECK_MODULES(SETTINGS_DAEMON,
        glib-2.0 >= $GLIB_REQUIRED_VERSION
        gtk+-3.0 >= $GTK_REQUIRED_VERSION
        gio-2.0 >= $GIO_REQUIRED_VERSION
        gmodule-2.0
        gthread-2.0
	cinnamon-desktop
)

PKG_CHECK_MODULES(SETTINGS_PLUGIN,
        gtk+-3.0 >= $GTK_REQUIRED_VERSION
        gio-2.0 >= $GIO_REQUIRED_VERSION
        libnotify >= $LIBNOTIFY_REQUIRED_VERSION
        cinnamon-desktop
        x11
)

PKG_CHECK_MODULES(XI23, gtk+-3.0 >= $GTK_XINPUT_2_3_VERSION, have_xi_23=yes, have_xi_23=no)
if test x$have_xi_23 = xyes ; then
  AC_DEFINE(XI_23, 1, [Defined if GTK version is higher than 3.7.8])
fi

CSD_PLUGIN_LDFLAGS="-export_dynamic -module -avoid-version -no-undefined"
case $host_os in
  darwin*)
    CSD_PLUGIN_LDFLAGS="${CSD_PLUGIN_LDFLAGS} -Wl,-bundle_loader,\$(top_builddir)/cinnamon-settings-daemon/cinnamon-settings-daemon"
    ;;
esac
AC_SUBST([CSD_PLUGIN_LDFLAGS])

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl ================================================================
dnl GSettings stuff
dnl ================================================================

GLIB_GSETTINGS

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
dnl - Check for GLib 2.38 or more
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(NEW_GLIB, glib-2.0 >= 2.38.0, have_new_glib=yes, have_new_gib=no)
if test x$have_new_glib = xyes; then
        AC_DEFINE(HAVE_NEW_GLIB,1,[Got new glib-2.0])
fi

dnl ---------------------------------------------------------------------------
dnl - Check for libnotify
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(LIBNOTIFY, libnotify >= $LIBNOTIFY_REQUIRED_VERSION,
		  [have_libnotify=yes], have_libnotify=no)
if test "x$have_libnotify" = xno ; then
	AC_MSG_ERROR([libnotify is required to build cinnamon-settings-daemon])
fi
AC_SUBST(LIBNOTIFY_CFLAGS)
AC_SUBST(LIBNOTIFY_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Check for D-Bus
dnl ---------------------------------------------------------------------------

dnl - Are we specifying a different dbus root ?
AC_ARG_WITH(dbus-sys,
        [AC_HELP_STRING([--with-dbus-sys=<dir>],
        [where D-BUS system.d directory is])])
if ! test -z "$with_dbus_sys" ; then
        DBUS_SYS_DIR="$with_dbus_sys"
else
        DBUS_SYS_DIR='${datadir}/dbus-1/system.d'
fi
AC_SUBST(DBUS_SYS_DIR)

AC_ARG_WITH(dbus-services,
        [AC_HELP_STRING([--with-dbus-services=<dir>],
        [where D-BUS services directory is])])
if ! test -z "$with_dbus_services" ; then
        DBUS_SERVICES_DIR="$with_dbus_services"
else
        DBUS_SERVICES_DIR='${datadir}/dbus-1/system-services'
fi
AC_SUBST(DBUS_SERVICES_DIR)

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

PKG_CHECK_MODULES(BACKGROUND, x11 cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION)

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

PKG_CHECK_MODULES(XSETTINGS, fontconfig cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION)

dnl ---------------------------------------------------------------------------
dnl - Keyboard plugin stuff
dnl ---------------------------------------------------------------------------

LIBGNOMEKBD_REQUIRED=3.6.0
PKG_CHECK_MODULES(KEYBOARD, [libgnomekbdui >= $LIBGNOMEKBD_REQUIRED libgnomekbd >= $LIBGNOMEKBD_REQUIRED libxklavier >= 5.0 kbproto])

dnl ---------------------------------------------------------------------------
dnl - Housekeeping plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(GIOUNIX, [gio-unix-2.0])

dnl ---------------------------------------------------------------------------
dnl - media-keys plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(MEDIA_KEYS, [gio-unix-2.0 libnotify libcanberra $GUDEV_PKG cvc >= $CINNAMON_DESKTOP_REQUIRED_VERSION cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION])

dnl ---------------------------------------------------------------------------
dnl - xrandr plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(XRANDR, [cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION upower-glib >= $UPOWER_GLIB_REQUIRED_VERSION])

dnl ---------------------------------------------------------------------------
dnl - orientation plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(ORIENTATION, [cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION])

dnl ---------------------------------------------------------------------------
dnl - sound plugin stuff
dnl ---------------------------------------------------------------------------

PKG_CHECK_MODULES(SOUND, [libpulse >= $PA_REQUIRED_VERSION $GUDEV_PKG libpulse-mainloop-glib >= $PA_REQUIRED_VERSION] libcanberra)

# ---------------------------------------------------------------------------
# Power
# ---------------------------------------------------------------------------
PKG_CHECK_MODULES(POWER, upower-glib >= $UPOWER_GLIB_REQUIRED_VERSION cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION libcanberra-gtk3 libnotify x11 xext)

if test x$have_gudev != xno; then
	PKG_CHECK_MODULES(BACKLIGHT_HELPER,
		glib-2.0 >= $GLIB_REQUIRED_VERSION
		gudev-1.0
	)
fi

dnl ---------------------------------------------------------------------------
dnl - color
dnl ---------------------------------------------------------------------------

AC_ARG_ENABLE(color,
              AS_HELP_STRING([--disable-color], [disable Colord support (default: enabled)]),,
              enable_color=yes)
build_color=false
if test x"$enable_color" != x"no" ; then
  PKG_CHECK_MODULES(COLOR, [colord >= 0.1.27 cinnamon-desktop >= $CINNAMON_DESKTOP_REQUIRED_VERSION libcanberra-gtk3])
  build_color=true
fi

AM_CONDITIONAL(BUILD_COLOR, [test "x$build_color" = "xtrue"])

dnl ---------------------------------------------------------------------------
dnl - wacom
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(wacom,
              AS_HELP_STRING([--disable-wacom], [disable Wacom support (default: auto)]),,
              enable_wacom=auto)
build_wacom=false
if test x"$enable_wacom" != x"no" ; then
  PKG_CHECK_MODULES(WACOM, [libwacom >= $LIBWACOM_REQUIRED_VERSION x11 xi xtst gudev-1.0 cinnamon-desktop xorg-wacom librsvg-2.0 >= $LIBRSVG_REQUIRED_VERSION gtk+-3.0 >= 3.8.0],
                    [build_wacom="true" AC_DEFINE(HAVE_WACOM, 1, [Define if wacom is being build])], [build_wacom="false"])
fi

if test x$enable_wacom = xyes; then
  if test x$build_wacom = xfalse; then
    AC_MSG_ERROR(Wacom support requested but required dependencies not found)
  fi
fi

AM_CONDITIONAL(BUILD_WACOM, test "x$build_wacom" = "xtrue")

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

have_smartcard_support=false
if test x$WANT_SMARTCARD_SUPPORT = xyes ; then
       NSS_REQUIRED_VERSION=3.11.2
       PKG_CHECK_MODULES(NSS, [nss >= $NSS_REQUIRED_VERSION])
       have_smartcard_support=true
       AC_DEFINE(SMARTCARD_SUPPORT, 1, [Define if smartcard support should be enabled])
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

# ---------------------------------------------------------------------------
# PolicyKit for the date & time mechanism
# ---------------------------------------------------------------------------

POLKIT_REQUIRED=0.97
DBUS_REQUIRED=1.1.2

# PolicyKit detection; defaults to 'auto' (use it if it's available)
#
POLKIT_CFLAGS=
POLKIT_LIBS=
AC_ARG_ENABLE(polkit,
              AS_HELP_STRING([--enable-polkit],
                             [Enable PolicyKit support (auto)]),
              enable_polkit=$enableval,
              enable_polkit=auto)

if test "x$enable_polkit" = "xno" ; then
  HAVE_POLKIT=no
else
  HAVE_POLKIT=no
  PKG_CHECK_MODULES(POLKIT, polkit-gobject-1 >= $POLKIT_REQUIRED dbus-1 >= $DBUS_REQUIRED dbus-glib-1, HAVE_POLKIT=yes, HAVE_POLKIT=no)

  if test "x$enable_polkit" = "xyes" -a "x$HAVE_POLKIT" = "xno" ; then
    AC_MSG_ERROR(PolicyKit support explicity enabled but not available)
  fi

  if test "x$HAVE_POLKIT" = "xyes" ; then
    AC_DEFINE(HAVE_POLKIT, 1, [Defined if PolicyKit support is enabled])
  fi
fi
AM_CONDITIONAL(HAVE_POLKIT, test "x$HAVE_POLKIT" = "xyes")
AC_SUBST(POLKIT_CFLAGS)
AC_SUBST(POLKIT_LIBS)

dnl ====================================================================
dnl Check for logind
dnl ====================================================================

AC_ARG_ENABLE([logind],
              AS_HELP_STRING([--disable-logind], [Do not check for logind]),
              [enable_logind=$enableval],
              [enable_logind=auto])

have_logind=no
if test x$enable_logind != xno ; then
    PKG_CHECK_MODULES(LOGIND, [libsystemd-login], [have_logind=yes], [
        PKG_CHECK_MODULES(LOGIND, [libsystemd], [have_logind=yes], [
            PKG_CHECK_MODULES(LOGIND, [libelogind], [have_logind=yes], [have_logind=no])
	])
    ])
fi

if test x$have_logind = xyes; then
    AC_DEFINE(HAVE_LOGIND, 1, [Define if logind is supported])
else
    if test x$enable_logind = xyes; then
        AC_MSG_ERROR([logind support requested but neither systemd or elogind were not found])
    fi
fi

AC_SUBST(LOGIND_CFLAGS)
AC_SUBST(LOGIND_LIBS)

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

plugindir='$(libdir)/cinnamon-settings-daemon-gsd_api_version'
AC_SUBST([plugindir])

PLUGIN_CFLAGS="-DG_LOG_DOMAIN=\"\\\"\$(plugin_name)-plugin\\\"\" -DPLUGIN_NAME=\"\\\"\$(plugin_name)\\\"\" "
AC_SUBST(PLUGIN_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl - Finish
dnl ---------------------------------------------------------------------------

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

CFLAGS="$CFLAGS -Wno-deprecated-declarations -Wno-deprecated -Wno-declaration-after-statement"

AC_OUTPUT([
Makefile
cinnamon-settings-daemon/Makefile
files/Makefile
plugins/Makefile
plugins/a11y-keyboard/Makefile
plugins/a11y-settings/Makefile
plugins/automount/Makefile
plugins/background/Makefile
plugins/clipboard/Makefile
plugins/color/Makefile
plugins/common/Makefile
plugins/cursor/Makefile
plugins/datetime/Makefile
plugins/dummy/Makefile
plugins/power/Makefile
plugins/housekeeping/Makefile
plugins/keyboard/Makefile
plugins/media-keys/Makefile
plugins/mouse/Makefile
plugins/orientation/Makefile
plugins/print-notifications/Makefile
plugins/screensaver-proxy/Makefile
plugins/smartcard/Makefile
plugins/sound/Makefile
plugins/wacom/Makefile
plugins/xrandr/Makefile
plugins/xsettings/Makefile
data/Makefile
data/cinnamon-settings-daemon.pc
data/org.cinnamon.settings-daemon.plugins.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.xsettings.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.power.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.color.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.media-keys.gschema.xml.in
data/org.cinnamon.settings-daemon.peripherals.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.housekeeping.gschema.xml.in
data/org.cinnamon.settings-daemon.peripherals.wacom.gschema.xml.in
data/org.cinnamon.settings-daemon.plugins.xrandr.gschema.xml.in
po/Makefile.in
])

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
                    cinnamon-settings-daemon $VERSION
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

        dbus-1 system.d dir:      ${DBUS_SYS_DIR}
        PolicyKit support:        ${HAVE_POLKIT}

        Logind supported:         ${have_logind}
        LCMS DICT support:        ${have_new_lcms}
        Libnotify support:        ${have_libnotify}

        Colord support:           ${build_color}
        Wacom support:            ${build_wacom}

        Smartcard support:        ${have_smartcard_support}
        Cups support:             ${enable_cups}
${NSS_DATABASE:+\
        System nssdb:             ${NSS_DATABASE}
}\
        Profiling support:        ${enable_profiling}
"
