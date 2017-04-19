plugin_name = sound

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-sound

csd_sound_SOURCES =	\
	csd-sound-manager.h	\
	csd-sound-manager.c	\
	main.c

csd_sound_CFLAGS =						\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(SOUND_CFLAGS)						\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_sound_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SOUND_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-sound.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-sound.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
