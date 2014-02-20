NULL =

plugin_name = automount

libexec_PROGRAMS = csd-test-automount

csd_test_automount_SOURCES = 		\
	test-automount.c		\
	csd-automount-manager.h		\
	csd-automount-manager.c		\
	csd-autorun.c		\
	csd-autorun.h		\
	$(NULL)

csd_test_automount_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

csd_test_automount_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(SYSTEMD_CFLAGS)		\
	$(AUTOMOUNT_CFLAGS)
	$(AM_CFLAGS)

csd_test_automount_LDADD =			\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la	\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(SYSTEMD_LIBS)			\
	$(AUTOMOUNT_LIBS)		\
	$(NULL)

plugin_LTLIBRARIES = \
	libautomount.la		\
	$(NULL)

libautomount_la_SOURCES =		\
	csd-automount-plugin.h		\
	csd-automount-plugin.c		\
	csd-automount-manager.h		\
	csd-automount-manager.c		\
	csd-autorun.c		\
	csd-autorun.h		\
	$(NULL)

libautomount_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libautomount_la_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(SYSTEMD_CFLAGS)		\
	$(AUTOMOUNT_CFLAGS)		\
	$(AM_CFLAGS)

libautomount_la_LDFLAGS =		\
	$(CSD_PLUGIN_LDFLAGS)		\
	$(NULL)

libautomount_la_LIBADD  =		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(SYSTEMD_LIBS)			\
	$(AUTOMOUNT_LIBS)		\
	$(NULL)

plugin_in_files =		\
	automount.cinnamon-settings-plugin.in	\
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
