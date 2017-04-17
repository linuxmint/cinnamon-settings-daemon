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

libexec_PROGRAMS = csd-housekeeping

csd_housekeeping_SOURCES =	\
	main.c	\
	$(COMMON_FILES)			\
	csd-housekeeping-manager.c	\
	csd-housekeeping-manager.h

csd_housekeeping_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(GIOUNIX_CFLAGS)		\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

csd_housekeeping_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_housekeeping_LDADD  = 	\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(GIOUNIX_LIBS)			\
	$(LIBNOTIFY_LIBS)		\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-housekeeping.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-housekeeping.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 	\
	$(desktop_in_files)

CLEANFILES = 	\
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
