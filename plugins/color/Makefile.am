plugin_name = color

AM_CFLAGS = $(WARN_CFLAGS)

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

libexec_PROGRAMS = csd-color
csd_color_SOURCES =		\
	gcm-profile-store.c		\
	gcm-profile-store.h		\
	gcm-dmi.c			\
	gcm-dmi.h			\
	gcm-edid.c			\
	gcm-edid.h			\
	csd-color-manager.c		\
	csd-color-manager.h		\
	main.c

csd_color_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(COLOR_CFLAGS)		\
	$(LCMS_CFLAGS)			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LIBNOTIFY_CFLAGS)		\
	$(AM_CFLAGS)

csd_color_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DBINDIR=\"$(bindir)\"				\
	$(AM_CPPFLAGS)

csd_color_LDADD  = 			\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la	\
	$(COLOR_LIBS)			\
	$(LCMS_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la -lm


desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-color.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-color.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 					\
	$(desktop_in_files)			\
	test-data/Lenovo-T61-Internal.bin	\
	test-data/LG-L225W-External.bin

CLEANFILES = 				\
	$(desktop_DATA)

DISTCLEANFILES =			\
	$(desktop_DATA)
