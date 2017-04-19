plugin_name = print-notifications

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-printer

csd_printer_SOURCES = 	\
	csd-printer.c

csd_printer_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)					\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CFLAGS)

csd_printer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(CUPS_LIBS)		\
	$(LIBNOTIFY_LIBS)

libexec_PROGRAMS += csd-print-notifications

csd_print_notifications_SOURCES =		\
	csd-print-notifications-manager.c	\
	csd-print-notifications-manager.h	\
	main.c

csd_print_notifications_CPPFLAGS =				\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_print_notifications_CFLAGS =	\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_print_notifications_LDADD =				\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(CUPS_LIBS)						\
	$(LIBNOTIFY_LIBS)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-print-notifications.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-print-notifications.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
