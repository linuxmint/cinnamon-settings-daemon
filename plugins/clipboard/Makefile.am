NULL =

plugin_name = clipboard

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-clipboard

csd_clipboard_SOURCES =	\
	main.c	\
	csd-clipboard-manager.h	\
	csd-clipboard-manager.c	\
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(NULL)

csd_clipboard_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_clipboard_CPPFLAGS = \
	$(PLUGIN_CFLAGS)		\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

csd_clipboard_LDADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-clipboard.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-clipboard.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	$(desktop_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(NULL)
