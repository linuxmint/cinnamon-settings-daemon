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

plugin_in_files = 		\
	clipboard.cinnamon-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 			\
	$(plugin_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@
