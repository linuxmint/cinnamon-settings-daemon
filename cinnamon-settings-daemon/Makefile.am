NULL =

INCLUDES = \
	-DDATADIR=\""$(datadir)"\"				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DCINNAMON_SETTINGS_PLUGINDIR=\""$(plugindir)"\"		\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(SETTINGS_DAEMON_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS)					\
	$(SYSTEMD_CFLAGS)					\
	$(NULL)

privlibdir = $(pkglibdir)-$(CSD_API_VERSION)

privlib_LTLIBRARIES =		\
	libcsd.la		\
	$(NULL)

libcsd_la_SOURCES =		\
	cinnamon-settings-profile.c	\
	cinnamon-settings-profile.h	\
	cinnamon-settings-session.c	\
	cinnamon-settings-session.h	\
	$(NULL)

libcsd_la_CPPFLAGS = 		\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(NULL)

libcsd_la_CFLAGS =		\
	$(NULL)

libcsd_la_LIBADD =		\
	$(SYSTEMD_LIBS)		\
	$(GIOUNIX_LIBS)		\
	$(NULL)

libcsd_la_LDFLAGS =		\
	-export-dynamic		\
	-avoid-version		\
	-no-undefined		\
	$(NULL)

csddir = $(libexecdir)

csd_PROGRAMS = \
	cinnamon-settings-daemon

apidir   = $(includedir)/cinnamon-settings-daemon-$(CSD_API_VERSION)/cinnamon-settings-daemon
api_DATA = 				\
	cinnamon-settings-plugin.h

cinnamon_settings_daemon_SOURCES =		\
	main.c				\
	cinnamon-settings-manager.c	\
	cinnamon-settings-manager.h	\
	cinnamon-settings-plugin.c		\
	cinnamon-settings-plugin.h		\
	cinnamon-settings-plugin-info.c	\
	cinnamon-settings-plugin-info.h	\
	cinnamon-settings-module.c		\
	cinnamon-settings-module.h		\
	$(NULL)

cinnamon_settings_daemon_CPPFLAGS = \
	$(AM_CPPFLAGS)

cinnamon_settings_daemon_CFLAGS = 		\
	$(AM_CFLAGS)

cinnamon_settings_daemon_LDFLAGS = 	\
	$(AM_LDFLAGS)

cinnamon_settings_daemon_LDADD = 		\
	libcsd.la		\
	$(SETTINGS_DAEMON_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(GNOME_DESKTOP_LIBS)		\
	$(NULL)

# vim: ts=8
