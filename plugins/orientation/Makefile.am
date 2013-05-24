plugin_name = orientation

libexec_PROGRAMS = csd-test-orientation

csd_test_orientation_SOURCES =		\
	csd-orientation-manager.h	\
	csd-orientation-manager.c	\
	test-orientation.c

csd_test_orientation_CFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(ORIENTATION_CFLAGS)					\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_test_orientation_LDADD =					\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(ORIENTATION_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_LTLIBRARIES = liborientation.la

liborientation_la_SOURCES = 		\
	csd-orientation-plugin.h	\
	csd-orientation-plugin.c	\
	csd-orientation-manager.h	\
	csd-orientation-manager.c

liborientation_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-I$(top_srcdir)/data/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

liborientation_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(ORIENTATION_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

liborientation_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

liborientation_la_LIBADD  =						\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(ORIENTATION_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = orientation.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
