plugin_name = screensaver-proxy

plugin_LTLIBRARIES = libscreensaver-proxy.la

libscreensaver_proxy_la_SOURCES =	\
	csd-screensaver-proxy-manager.c	\
	csd-screensaver-proxy-manager.h	\
	csd-screensaver-proxy-plugin.c	\
	csd-screensaver-proxy-plugin.h

libscreensaver_proxy_la_CPPFLAGS =				\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libscreensaver_proxy_la_CFLAGS =	\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libscreensaver_proxy_la_LDFLAGS = $(CSD_PLUGIN_LDFLAGS)

libscreensaver_proxy_la_LIBADD  = $(SETTINGS_PLUGIN_LIBS)

libexec_PROGRAMS = csd-test-screensaver-proxy

csd_test_screensaver_proxy_SOURCES =		\
	test-screensaver-proxy.c			\
	csd-screensaver-proxy-manager.c		\
	csd-screensaver-proxy-manager.h

csd_test_screensaver_proxy_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_test_screensaver_proxy_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_test_screensaver_proxy_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = screensaver-proxy.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)

DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
