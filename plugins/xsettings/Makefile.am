NULL =

plugin_name = xsettings

noinst_PROGRAMS = test-gtk-modules

test_gtk_modules_SOURCES =	\
	gsd-xsettings-gtk.c	\
	gsd-xsettings-gtk.h	\
	test-gtk-modules.c

test_gtk_modules_CFLAGS =		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

test_gtk_modules_LDADD =					\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(XSETTINGS_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

test_gtk_modules_CPPFLAGS =				\
	-I$(top_srcdir)/gnome-settings-daemon		\
	-I$(top_srcdir)/data/				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTK_MODULES_DIRECTORY=\""$(libdir)/gnome-settings-daemon-@GSD_API_VERSION@/gtk-modules/"\" \
	$(AM_CPPFLAGS)

libexec_PROGRAMS = gsd-test-xsettings

gsd_test_xsettings_SOURCES =	\
	gsd-xsettings-gtk.c	\
	gsd-xsettings-gtk.h	\
	gsd-xsettings-manager.c	\
	gsd-xsettings-gtk.h	\
	xsettings-common.c	\
	xsettings-common.h	\
	xsettings-manager.c	\
	xsettings-manager.h	\
	fontconfig-monitor.c	\
	fontconfig-monitor.h	\
	test-xsettings.c

gsd_test_xsettings_CFLAGS = $(test_gtk_modules_CFLAGS)
gsd_test_xsettings_CPPFLAGS = $(test_gtk_modules_CPPFLAGS) -I$(top_srcdir)/plugins/common
gsd_test_xsettings_LDADD = $(test_gtk_modules_LDADD)

plugin_LTLIBRARIES = \
	libxsettings.la		\
	$(NULL)

libxsettings_la_SOURCES = 	\
	gsd-xsettings-plugin.h	\
	gsd-xsettings-plugin.c	\
	gsd-xsettings-manager.h	\
	gsd-xsettings-manager.c	\
	gsd-xsettings-gtk.c	\
	gsd-xsettings-gtk.h	\
	xsettings-common.h	\
	xsettings-common.c	\
	xsettings-manager.h	\
	xsettings-manager.c	\
	fontconfig-monitor.h	\
	fontconfig-monitor.c	\
	$(NULL)

libxsettings_la_CPPFLAGS =					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DGTK_MODULES_DIRECTORY=\""$(libdir)/gnome-settings-daemon-@GSD_API_VERSION@/gtk-modules/"\" \
	$(AM_CPPFLAGS)

libxsettings_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(XSETTINGS_CFLAGS)		\
	$(AM_CFLAGS)

libxsettings_la_LDFLAGS = 	\
	$(GSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libxsettings_la_LIBADD  =					\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(XSETTINGS_LIBS)					\
	$(NULL)

plugin_in_files = 		\
	xsettings.gnome-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

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

@GSD_INTLTOOL_PLUGIN_RULE@
