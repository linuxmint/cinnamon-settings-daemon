plugin_name = print-notifications

plugin_LTLIBRARIES = \
	libprint-notifications.la

libprint_notifications_la_SOURCES = 		\
	csd-print-notifications-manager.c	\
	csd-print-notifications-manager.h	\
	csd-print-notifications-plugin.c	\
	csd-print-notifications-plugin.h

libprint_notifications_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" \
	$(AM_CPPFLAGS)

libprint_notifications_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libprint_notifications_la_LDFLAGS = 		\
	$(CSD_PLUGIN_LDFLAGS)

libprint_notifications_la_LIBADD  = 		\
	$(CUPS_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 		\
	print-notifications.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

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

libexec_PROGRAMS += csd-test-print-notifications

csd_test_print_notifications_SOURCES =		\
	csd-print-notifications-manager.c	\
	csd-print-notifications-manager.h	\
	test-print-notifications.c

csd_test_print_notifications_CPPFLAGS =				\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_test_print_notifications_CFLAGS =	\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_test_print_notifications_LDADD =				\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
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

@CSD_INTLTOOL_PLUGIN_RULE@
