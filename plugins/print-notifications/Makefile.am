plugin_name = print-notifications

plugin_LTLIBRARIES = \
	libprint-notifications.la

libprint_notifications_la_SOURCES = 		\
	gsd-print-notifications-manager.c	\
	gsd-print-notifications-manager.h	\
	gsd-print-notifications-plugin.c	\
	gsd-print-notifications-plugin.h

libprint_notifications_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon		\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libprint_notifications_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libprint_notifications_la_LDFLAGS = 		\
	$(GSD_PLUGIN_LDFLAGS)

libprint_notifications_la_LIBADD  = 		\
	$(CUPS_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	print-notifications.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

libexec_PROGRAMS = gsd-printer

gsd_printer_SOURCES = 	\
	gsd-printer.c

gsd_printer_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)					\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CFLAGS)

gsd_printer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)	\
	$(CUPS_LIBS)		\
	$(LIBNOTIFY_LIBS)

libexec_PROGRAMS += gsd-test-print-notifications

gsd_test_print_notifications_SOURCES =		\
	gsd-print-notifications-manager.c	\
	gsd-print-notifications-manager.h	\
	test-print-notifications.c

gsd_test_print_notifications_CPPFLAGS =				\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

gsd_test_print_notifications_CFLAGS =	\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

gsd_test_print_notifications_LDADD =				\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(CUPS_LIBS)						\
	$(LIBNOTIFY_LIBS)

EXTRA_DIST = 			\
	$(plugin_in_files)

CLEANFILES = 			\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
