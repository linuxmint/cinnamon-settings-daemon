NULL =

plugin_name = clipboard

plugin_LTLIBRARIES = \
	libclipboard.la		\
	$(NULL)

libclipboard_la_SOURCES = 	\
	gsd-clipboard-plugin.h	\
	gsd-clipboard-plugin.c	\
	gsd-clipboard-manager.h	\
	gsd-clipboard-manager.c	\
	xutils.h		\
	xutils.c		\
	list.h			\
	list.c			\
	$(NULL)

libclipboard_la_CPPFLAGS = \
	$(PLUGIN_CFLAGS)		\
	-I$(top_srcdir)/gnome-settings-daemon		\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libclipboard_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libclipboard_la_LDFLAGS = 	\
	$(GSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libclipboard_la_LIBADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NULL)

plugin_in_files = 		\
	clipboard.gnome-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@GSD_INTLTOOL_PLUGIN_RULE@
