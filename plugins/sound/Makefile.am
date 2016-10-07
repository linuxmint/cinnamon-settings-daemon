plugin_name = sound

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-test-sound

csd_test_sound_SOURCES =	\
	csd-sound-manager.h	\
	csd-sound-manager.c	\
	test-sound.c

csd_test_sound_CFLAGS =						\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(SOUND_CFLAGS)						\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_test_sound_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SOUND_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_LTLIBRARIES = \
	libsound.la

libsound_la_SOURCES = \
	csd-sound-plugin.h \
	csd-sound-plugin.c \
	csd-sound-manager.h \
	csd-sound-manager.c

libsound_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon \
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libsound_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS) \
	$(SOUND_CFLAGS)	\
	$(AM_CFLAGS)

libsound_la_LDFLAGS = \
	$(WARN_LDFLAGS)		\
	$(CSD_PLUGIN_LDFLAGS)

libsound_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(SOUND_LIBS)

plugin_in_files = \
	sound.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
