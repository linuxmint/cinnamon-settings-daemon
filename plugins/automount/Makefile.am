NULL =

plugin_name = automount

libexec_PROGRAMS = csd-automount

AM_CFLAGS = $(WARN_CFLAGS)

csd_automount_SOURCES = 		\
	main.c		\
	csd-automount-manager.h		\
	csd-automount-manager.c		\
	csd-autorun.c		\
	csd-autorun.h		\
	$(NULL)

csd_automount_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

csd_automount_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(LOGIND_CFLAGS)		\
	$(AUTOMOUNT_CFLAGS)
	$(AM_CFLAGS)

csd_automount_LDADD =			\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la	\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(LOGIND_LIBS)			\
	$(AUTOMOUNT_LIBS)		\
	$(NULL)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-automount.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-automount.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 			\
	$(desktop_in_files)	\
	$(NULL)

CLEANFILES = 			\
	$(desktop_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(desktop_DATA)		\
	$(NULL)
