NULL =

plugin_name = xsettings

AM_CFLAGS = $(WARN_CFLAGS)

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

libexec_PROGRAMS = csd-xsettings

csd_xsettings_SOURCES =	\
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
	main.c

csd_xsettings_CFLAGS = $(test_gtk_modules_CFLAGS)
csd_xsettings_CPPFLAGS = $(test_gtk_modules_CPPFLAGS) -I$(top_srcdir)/plugins/common
csd_xsettings_LDADD = $(test_gtk_modules_LDADD)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-xsettings.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-xsettings.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	README.xsettings	\
	$(desktop_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(NULL)
