icondir = $(datadir)/icons/hicolor
context = actions

plugin_name = media-keys

NULL =
SUBDIRS =

AM_CFLAGS = $(WARN_CFLAGS)

BUILT_SOURCES = 			\
	csd-marshal.h			\
	csd-marshal.c			\
	$(NULL)

csd-marshal.c: csd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --header --body --internal > $@

csd-marshal.h: csd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=csd_marshal $< --header --internal > $@

libexec_PROGRAMS = csd-media-keys

csd_media_keys_SOURCES =			\
	csd-media-keys-manager.c		\
	csd-media-keys-manager.h		\
	bus-watch-namespace.c			\
	bus-watch-namespace.h			\
	mpris-controller.c		        \
	mpris-controller.h	        	\
	main.c			\
	$(BUILT_SOURCES)			\
	$(NULL)

csd_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_media_keys_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

csd_media_keys_LDADD = \
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MEDIA_KEYS_LIBS)			\
	-lm

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-media-keys.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-media-keys.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 				\
	csd-marshal.list		\
	README.media-keys-API		\
	$(desktop_in_files)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(desktop_DATA)

DISTCLEANFILES =		\
	$(desktop_DATA)
