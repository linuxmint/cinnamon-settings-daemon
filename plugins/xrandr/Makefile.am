plugin_name = xrandr

icondir = $(datadir)/icons/hicolor
context = apps

ICON_FILES = 			\
	gsd-xrandr-16.png	\
	gsd-xrandr-22.png	\
	gsd-xrandr-24.png	\
	gsd-xrandr-32.png	\
	gsd-xrandr.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/gnome-settings-daemon/xrandr
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/gsd-xrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/gsd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/gsd-xrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/gsd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/gsd-xrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/gsd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/gsd-xrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/gsd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/gsd-xrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/gsd-xrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/gsd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/gsd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/gsd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/gsd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/gsd-xrandr.svg

plugin_LTLIBRARIES = \
	libxrandr.la

libxrandr_la_SOURCES = 		\
	gsd-xrandr-plugin.h	\
	gsd-xrandr-plugin.c	\
	gsd-xrandr-manager.h	\
	gsd-xrandr-manager.c

libxrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/plugins/common/				\
	-DBINDIR=\"$(bindir)\"					\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libxrandr_la_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(XRANDR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

libxrandr_la_LDFLAGS = 			\
	$(GSD_PLUGIN_LDFLAGS)

libxrandr_la_LIBADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(XRANDR_LIBS)					\
	$(WACOM_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files =			\
	xrandr.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
