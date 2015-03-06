plugin_name = rfkill

libexec_PROGRAMS = csd-test-rfkill

csd_test_rfkill_SOURCES =		\
	csd-rfkill-manager.h	\
	csd-rfkill-manager.c	\
	rfkill-glib.c		\
	rfkill-glib.h		\
	rfkill.h		\
	test-rfkill.c

csd_test_rfkill_CFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_builddir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

csd_test_rfkill_LDADD =					\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_PLUGIN_LIBS)

plugin_LTLIBRARIES = librfkill.la

librfkill_la_SOURCES = 		\
	csd-rfkill-plugin.c	\
	csd-rfkill-manager.h	\
	csd-rfkill-manager.c	\
	rfkill-glib.c		\
	rfkill-glib.h		\
	rfkill.h

librfkill_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_builddir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/data/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

librfkill_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(RFKILL_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

librfkill_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

librfkill_la_LIBADD  =						\
	$(RFKILL_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = rfkill.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

udevrulesdir = $(prefix)/lib/udev/rules.d
udevrules_DATA = 61-cinnamon-settings-daemon-rfkill.rules

EXTRA_DIST = $(plugin_in_files) $(udevrules_DATA)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
