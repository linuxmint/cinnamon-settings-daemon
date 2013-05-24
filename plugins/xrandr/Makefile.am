plugin_name = xrandr

icondir = $(datadir)/icons/hicolor
context = apps

ICON_FILES = 			\
	csd-xrandr-16.png	\
	csd-xrandr-22.png	\
	csd-xrandr-24.png	\
	csd-xrandr-32.png	\
	csd-xrandr.svg

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/cinnamon-settings-daemon/xrandr
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/csd-xrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/csd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd-xrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/csd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd-xrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/csd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd-xrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/csd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/csd-xrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/csd-xrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/csd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/csd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/csd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/csd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/csd-xrandr.svg

plugin_LTLIBRARIES = \
	libxrandr.la

libxrandr_la_SOURCES = 		\
	csd-xrandr-plugin.h	\
	csd-xrandr-plugin.c	\
	csd-xrandr-manager.h	\
	csd-xrandr-manager.c

libxrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/plugins/common/				\
	-DBINDIR=\"$(bindir)\"					\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libxrandr_la_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(XRANDR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

libxrandr_la_LDFLAGS = 			\
	$(CSD_PLUGIN_LDFLAGS)

libxrandr_la_LIBADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(XRANDR_LIBS)					\
	$(WACOM_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files =			\
	xrandr.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
