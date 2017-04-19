plugin_name = cursor

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-cursor

csd_cursor_SOURCES =		\
	main.c					\
	csd-cursor-manager.c	\
	csd-cursor-manager.h

csd_cursor_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_cursor_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(CURSOR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_cursor_LDADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(CURSOR_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-cursor.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-cursor.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
