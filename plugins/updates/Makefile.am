plugin_name = updates

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = org.cinnamon.SettingsDaemonUpdates.xml

plugin_LTLIBRARIES = \
	libupdates.la

libupdates_la_SOURCES = \
	csd-updates-common.h \
	csd-updates-plugin.h \
	csd-updates-plugin.c \
	csd-updates-refresh.h \
	csd-updates-refresh.c \
	csd-updates-firmware.h \
	csd-updates-firmware.c \
	csd-updates-manager.h \
	csd-updates-manager.c

libupdates_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon \
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

libupdates_la_CFLAGS = \
	$(PLUGIN_CFLAGS) \
	$(SETTINGS_PLUGIN_CFLAGS) \
	$(GUDEV_CFLAGS)	\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE \
	$(PACKAGEKIT_CFLAGS) \
	-DI_KNOW_THE_PACKAGEKIT_GLIB2_API_IS_SUBJECT_TO_CHANGE \
	-DDATADIR=\"$(datadir)\" \
	-DBINDIR=\"$(bindir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	-I$(top_srcdir)/data \
	$(AM_CFLAGS)

libupdates_la_LDFLAGS = \
	$(CSD_PLUGIN_LDFLAGS)

libupdates_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(PACKAGEKIT_LIBS)

plugin_in_files = \
	updates.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
