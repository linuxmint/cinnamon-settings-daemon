icondir = $(datadir)/icons/hicolor
context = actions

plugin_name = media-keys

NULL =
SUBDIRS =

plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	csd-marshal.h			\
	csd-marshal.c			\
	$(NULL)

csd-marshal.c: csd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --header --body --internal > $@

csd-marshal.h: csd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --header --internal > $@

libmedia_keys_la_SOURCES = 		\
	csd-media-keys-plugin.h		\
	csd-media-keys-plugin.c		\
	csd-media-keys-manager.h	\
	csd-media-keys-manager.c	\
	bus-watch-namespace.c		\
	bus-watch-namespace.h		\
	mpris-controller.c		\
	mpris-controller.h		\
	$(BUILT_SOURCES)		\
	$(NULL)

libmedia_keys_la_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libmedia_keys_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

libmedia_keys_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = 		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(MEDIA_KEYS_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)						\
	-lm

plugin_in_files = 		\
	media-keys.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

libexec_PROGRAMS = csd-test-media-keys

csd_test_media_keys_SOURCES =			\
	csd-media-keys-manager.c		\
	csd-media-keys-manager.h		\
	bus-watch-namespace.c			\
	bus-watch-namespace.h			\
	mpris-controller.c		        \
	mpris-controller.h	        	\
	test-media-keys.c			\
	$(BUILT_SOURCES)			\
	$(NULL)

csd_test_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_test_media_keys_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

csd_test_media_keys_LDADD = \
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MEDIA_KEYS_LIBS)			\
	-lm

EXTRA_DIST = 				\
	csd-marshal.list		\
	README.media-keys-API		\
	$(plugin_in_files)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
