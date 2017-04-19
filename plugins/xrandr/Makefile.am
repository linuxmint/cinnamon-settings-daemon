plugin_name = xrandr

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-xrandr

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/cinnamon-settings-daemon/xrandr

csd_xrandr_SOURCES = 		\
	main.c	\
	csd-xrandr-manager.h	\
	csd-xrandr-manager.c

csd_xrandr_CPPFLAGS =						\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/plugins/common/				\
	-DBINDIR=\"$(bindir)\"					\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_xrandr_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(XRANDR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

csd_xrandr_LDADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la	\
	$(XRANDR_LIBS)					\
	$(WACOM_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-xrandr.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-xrandr.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(desktop_in_files) $(ICON_FILES)
CLEANFILES = $(desktop_DATA)
DISTCLEANFILES = $(desktop_DATA)
