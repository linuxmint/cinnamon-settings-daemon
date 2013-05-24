NULL =

plugin_name = xsettings

noinst_PROGRAMS = test-gtk-modules

test_gtk_modules_SOURCES =	\
	csd-xsettings-gtk.c	\
	csd-xsettings-gtk.h	\
	test-gtk-modules.c

test_gtk_modules_CFLAGS =		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

test_gtk_modules_LDADD =					\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(XSETTINGS_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

test_gtk_modules_CPPFLAGS =				\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/data/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTK_MODULES_DIRECTORY=\""$(libdir)/cinnamon-settings-daemon-@CSD_API_VERSION@/gtk-modules/"\" \
	$(AM_CPPFLAGS)

libexec_PROGRAMS = csd-test-xsettings

csd_test_xsettings_SOURCES =	\
	csd-xsettings-gtk.c	\
	csd-xsettings-gtk.h	\
	csd-xsettings-manager.c	\
	csd-xsettings-gtk.h	\
	xsettings-common.c	\
	xsettings-common.h	\
	xsettings-manager.c	\
	xsettings-manager.h	\
	fontconfig-monitor.c	\
	fontconfig-monitor.h	\
	test-xsettings.c

csd_test_xsettings_CFLAGS = $(test_gtk_modules_CFLAGS)
csd_test_xsettings_CPPFLAGS = $(test_gtk_modules_CPPFLAGS) -I$(top_srcdir)/plugins/common
csd_test_xsettings_LDADD = $(test_gtk_modules_LDADD)

plugin_LTLIBRARIES = \
	libxsettings.la		\
	$(NULL)

libxsettings_la_SOURCES = 	\
	csd-xsettings-plugin.h	\
	csd-xsettings-plugin.c	\
	csd-xsettings-manager.h	\
	csd-xsettings-manager.c	\
	csd-xsettings-gtk.c	\
	csd-xsettings-gtk.h	\
	xsettings-common.h	\
	xsettings-common.c	\
	xsettings-manager.h	\
	xsettings-manager.c	\
	fontconfig-monitor.h	\
	fontconfig-monitor.c	\
	$(NULL)

libxsettings_la_CPPFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DGTK_MODULES_DIRECTORY=\""$(libdir)/cinnamon-settings-daemon-@CSD_API_VERSION@/gtk-modules/"\" \
	$(AM_CPPFLAGS)

libxsettings_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(XSETTINGS_CFLAGS)		\
	$(AM_CFLAGS)

libxsettings_la_LDFLAGS = 	\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libxsettings_la_LIBADD  =					\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(XSETTINGS_LIBS)					\
	$(NULL)

plugin_in_files = 		\
	xsettings.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 			\
	README.xsettings	\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@
