plugin_name = updates

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = org.gnome.SettingsDaemonUpdates.xml

plugin_LTLIBRARIES = \
	libupdates.la

libupdates_la_SOURCES = \
	gsd-updates-common.h \
	gsd-updates-plugin.h \
	gsd-updates-plugin.c \
	gsd-updates-refresh.h \
	gsd-updates-refresh.c \
	gsd-updates-firmware.h \
	gsd-updates-firmware.c \
	gsd-updates-manager.h \
	gsd-updates-manager.c

libupdates_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon \
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
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
	$(GSD_PLUGIN_LDFLAGS)

libupdates_la_LIBADD = \
	$(SETTINGS_PLUGIN_LIBS)	\
	$(PACKAGEKIT_LIBS)

plugin_in_files = \
	updates.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = \
	$(plugin_in_files)

CLEANFILES = \
	$(plugin_DATA)

DISTCLEANFILES = \
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
