plugin_name = screensaver-proxy

plugin_LTLIBRARIES = libscreensaver-proxy.la

libscreensaver_proxy_la_SOURCES =	\
	gsd-screensaver-proxy-manager.c	\
	gsd-screensaver-proxy-manager.h	\
	gsd-screensaver-proxy-plugin.c	\
	gsd-screensaver-proxy-plugin.h

libscreensaver_proxy_la_CPPFLAGS =				\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libscreensaver_proxy_la_CFLAGS =	\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libscreensaver_proxy_la_LDFLAGS = $(GSD_PLUGIN_LDFLAGS)

libscreensaver_proxy_la_LIBADD  = $(SETTINGS_PLUGIN_LIBS)

libexec_PROGRAMS = gsd-test-screensaver-proxy

gsd_test_screensaver_proxy_SOURCES =		\
	test-screensaver-proxy.c			\
	gsd-screensaver-proxy-manager.c		\
	gsd-screensaver-proxy-manager.h

gsd_test_screensaver_proxy_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

gsd_test_screensaver_proxy_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

gsd_test_screensaver_proxy_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = screensaver-proxy.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)

DISTCLEANFILES = $(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
