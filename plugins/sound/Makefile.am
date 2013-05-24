plugin_name = sound

libexec_PROGRAMS = gsd-test-sound

gsd_test_sound_SOURCES =	\
	gsd-sound-manager.h	\
	gsd-sound-manager.c	\
	test-sound.c

gsd_test_sound_CFLAGS =						\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(SOUND_CFLAGS)						\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

gsd_test_sound_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SOUND_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_LTLIBRARIES = \
	libsound.la

libsound_la_SOURCES = \
	gsd-sound-plugin.h \
	gsd-sound-plugin.c \
	gsd-sound-manager.h \
	gsd-sound-manager.c

libsound_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon \
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libsound_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS) \
	$(SOUND_CFLAGS)	\
	$(AM_CFLAGS)

libsound_la_LDFLAGS = \
	$(GSD_PLUGIN_LDFLAGS)

libsound_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(SOUND_LIBS)

plugin_in_files = \
	sound.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
