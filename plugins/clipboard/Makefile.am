NULL =

plugin_name = clipboard

AM_CFLAGS = $(WARN_CFLAGS)

plugin_LTLIBRARIES = \
	libclipboard.la		\
	$(NULL)

libclipboard_la_SOURCES = 	\
	csd-clipboard-plugin.h	\
	csd-clipboard-plugin.c	\
	csd-clipboard-manager.h	\
	csd-clipboard-manager.c	\
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(NULL)

libclipboard_la_CPPFLAGS = \
	$(PLUGIN_CFLAGS)		\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libclipboard_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libclipboard_la_LDFLAGS = 	\
	$(WARN_LDFLAGS)		\
	$(CSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libclipboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

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

csd_clipboard_CFLAGS = $(libclipboard_la_CFLAGS)
csd_clipboard_CPPFLAGS = $(libclipboard_la_CPPFLAGS)
csd_clipboard_LDADD = $(libclipboard_la_LIBADD) $(top_builddir)/cinnamon-settings-daemon/libcsd.la

plugin_in_files = 		\
	clipboard.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-clipboard.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-clipboard.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(desktop_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(desktop_DATA)		\
	$(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@
