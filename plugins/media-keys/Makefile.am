icondir = $(datadir)/icons/hicolor
context = actions

plugin_name = media-keys

NULL =

SUBDIRS = cut-n-paste
plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	gsd-marshal.h			\
	gsd-marshal.c			\
	$(NULL)

gsd-marshal.c: gsd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=gsd_marshal $< --header --body --internal > $@

gsd-marshal.h: gsd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=gsd_marshal $< --header --internal > $@

libmedia_keys_la_SOURCES = 		\
	gsd-media-keys-plugin.h		\
	gsd-media-keys-plugin.c		\
	gsd-media-keys-manager.h	\
	gsd-media-keys-manager.c	\
	gsd-osd-window.c		\
	gsd-osd-window.h		\
	gsd-osd-window-private.h	\
	shortcuts-list.h		\
	$(BUILT_SOURCES)		\
	$(NULL)

libmedia_keys_la_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-I$(top_srcdir)/plugins/media-keys/cut-n-paste		\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libmedia_keys_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

libmedia_keys_la_LDFLAGS = 		\
	$(GSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = 		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(top_builddir)/plugins/media-keys/cut-n-paste/libgvc.la	\
	$(MEDIA_KEYS_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)						\
	-lm

plugin_in_files = 		\
	media-keys.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

noinst_PROGRAMS = test-media-window

test_media_window_SOURCES =			\
	gsd-osd-window.c			\
	gsd-osd-window.h			\
	gsd-osd-window-private.h		\
	test-media-window.c			\
	$(NULL)

test_media_window_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-I$(top_srcdir)/plugins/media-keys/cut-n-paste		\
	-DDATADIR=\""$(datadir)"\"				\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

test_media_window_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

test_media_window_LDADD = \
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	-lm

libexec_PROGRAMS = gsd-test-media-keys

gsd_test_media_keys_SOURCES =			\
	gsd-media-keys-manager.c		\
	gsd-media-keys-manager.h		\
	gsd-osd-window.c			\
	gsd-osd-window.h			\
	gsd-osd-window-private.h		\
	test-media-keys.c			\
	$(BUILT_SOURCES)			\
	$(NULL)

gsd_test_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-I$(top_srcdir)/plugins/media-keys/cut-n-paste		\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

gsd_test_media_keys_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

gsd_test_media_keys_LDADD = \
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(top_builddir)/plugins/media-keys/cut-n-paste/libgvc.la	\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MEDIA_KEYS_LIBS)			\
	-lm

EXTRA_DIST = 				\
	gsd-marshal.list		\
	README.media-keys-API		\
	test-media-window-clutter.c	\
	$(plugin_in_files)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
