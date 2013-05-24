plugin_name = color

plugin_LTLIBRARIES = \
	libcolor.la

libcolor_la_SOURCES = 			\
	gcm-profile-store.c		\
	gcm-profile-store.h		\
	gcm-dmi.c			\
	gcm-dmi.h			\
	gcm-edid.c			\
	gcm-edid.h			\
	csd-color-manager.c		\
	csd-color-manager.h		\
	csd-color-plugin.c		\
	csd-color-plugin.h

libcolor_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DBINDIR=\"$(bindir)\"				\
	$(AM_CPPFLAGS)

libcolor_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(COLOR_CFLAGS)		\
	$(LCMS_CFLAGS)			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

libcolor_la_LDFLAGS = 			\
	$(CSD_PLUGIN_LDFLAGS)

libcolor_la_LIBADD  = 			\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la	\
	$(COLOR_LIBS)			\
	$(LCMS_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LIBNOTIFY_LIBS)

check_PROGRAMS =			\
	gcm-self-test

gcm_self_test_CPPFLAGS = \
	-DTESTDATADIR=\""$(top_srcdir)/plugins/color/test-data"\" \
	$(AM_CPPFLAGS)

gcm_self_test_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(COLOR_CFLAGS)			\
	$(PLUGIN_CFLAGS)		\
	$(AM_CFLAGS)

gcm_self_test_SOURCES =			\
	gcm-dmi.c			\
	gcm-dmi.h			\
	gcm-edid.c			\
	gcm-edid.h			\
	gcm-self-test.c

gcm_self_test_LDADD =			\
	$(COLOR_LIBS)			\
	$(LCMS_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)		\
	-lm

TESTS = gcm-self-test

plugin_in_files = 			\
	color.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = 					\
	$(plugin_in_files)			\
	test-data/Lenovo-T61-Internal.bin	\
	test-data/LG-L225W-External.bin

CLEANFILES = 				\
	$(plugin_DATA)

DISTCLEANFILES =			\
	$(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
