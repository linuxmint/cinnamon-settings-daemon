plugin_name = housekeeping

COMMON_FILES =				\
	csd-disk-space.c		\
	csd-disk-space.h		\
	csd-ldsm-dialog.c		\
	csd-ldsm-dialog.h		\
	csd-disk-space-helper.h		\
	csd-disk-space-helper.c

noinst_PROGRAMS = csd-disk-space-test csd-empty-trash-test

AM_CFLAGS = $(WARN_CFLAGS)

csd_disk_space_test_SOURCES =		\
	csd-disk-space-test.c		\
	$(COMMON_FILES)
csd_disk_space_test_LDADD = $(SETTINGS_PLUGIN_LIBS) $(GIOUNIX_LIBS) $(LIBNOTIFY_LIBS)
csd_disk_space_test_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

csd_empty_trash_test_SOURCES =		\
	csd-empty-trash-test.c		\
	$(COMMON_FILES)
csd_empty_trash_test_LDADD = $(SETTINGS_PLUGIN_LIBS) $(GIOUNIX_LIBS) $(LIBNOTIFY_LIBS)
csd_empty_trash_test_CFLAGS =		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

plugin_LTLIBRARIES = libhousekeeping.la

libhousekeeping_la_SOURCES =		\
	$(COMMON_FILES)			\
	csd-housekeeping-manager.c	\
	csd-housekeeping-manager.h	\
	csd-housekeeping-plugin.c	\
	csd-housekeeping-plugin.h

libhousekeeping_la_CPPFLAGS = 					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libhousekeeping_la_CFLAGS = 		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

libhousekeeping_la_LDFLAGS = $(WARN_LDFLAGS) $(CSD_PLUGIN_LDFLAGS)

libhousekeeping_la_LIBADD = $(SETTINGS_PLUGIN_LIBS) $(GIOUNIX_LIBS) $(LIBNOTIFY_LIBS)

plugin_in_files = housekeeping.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = $(plugin_in_files)

CLEANFILES = $(plugin_DATA)

DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
