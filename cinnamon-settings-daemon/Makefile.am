NULL =

AM_CPPFLAGS = \
	-DDATADIR=\""$(datadir)"\"				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DCINNAMON_SETTINGS_PLUGINDIR=\""$(plugindir)"\"	\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(SETTINGS_DAEMON_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)					\
	$(LOGIND_CFLAGS)					\
	$(NULL)

AM_CFLAGS = $(WARN_CFLAGS)

AM_LDFLAGS = $(WARN_LDFLAGS)

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
	$(AM_CPPFLAGS)			\
	$(NULL)

libcsd_la_CFLAGS =		\
	$(WARN_CFLAGS)		\
	$(NULL)

libcsd_la_LIBADD =		\
	$(LOGIND_LIBS)		\
	$(GIOUNIX_LIBS)		\
	$(NULL)

libcsd_la_LDFLAGS =		\
	$(WARN_LDFLAGS)		\
	-export-dynamic		\
	-avoid-version		\
	-no-undefined		\
	$(NULL)

# vim: ts=8
