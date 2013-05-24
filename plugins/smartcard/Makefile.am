plugin_name = smartcard

libexec_PROGRAMS = gsd-test-smartcard

gsd_test_smartcard_SOURCES =	\
	gsd-smartcard-manager.h	\
	gsd-smartcard-manager.c	\
	gsd-smartcard.h         \
	gsd-smartcard.c         \
	test-smartcard.c

gsd_test_smartcard_CFLAGS =					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(NSS_CFLAGS)						\
	$(PLUGIN_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(AM_CFLAGS)

gsd_test_smartcard_LDADD =					\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(NSS_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)

plugin_LTLIBRARIES = \
	libsmartcard.la

libsmartcard_la_SOURCES = \
	gsd-smartcard-plugin.h  \
	gsd-smartcard-plugin.c  \
	gsd-smartcard.h         \
	gsd-smartcard.c         \
	gsd-smartcard-manager.h \
	gsd-smartcard-manager.c

libsmartcard_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon \
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DGSD_SMARTCARD_MANAGER_NSS_DB=\""$(NSS_DATABASE)"\" \
	$(AM_CPPFLAGS)

libsmartcard_la_CFLAGS = \
	$(PLUGIN_CFLAGS)	\
	$(SETTINGS_PLUGIN_CFLAGS) \
	$(NSS_CFLAGS)	\
	$(AM_CFLAGS)

libsmartcard_la_LDFLAGS = \
	$(GSD_PLUGIN_LDFLAGS)

libsmartcard_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(NSS_LIBS)

@GSD_INTLTOOL_PLUGIN_RULE@

plugin_in_files = \
	smartcard.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)
