plugin_name = sharing

plugin_LTLIBRARIES = libsharing.la

libsharing_la_SOURCES =		\
	csd-sharing-manager.c	\
	csd-sharing-manager.h	\
	csd-sharing-enums.h	\
	csd-sharing-plugin.c    \
        csd-sharing-plugin.h

libsharing_la_CPPFLAGS =					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_builddir)/cinnamon-settings-daemon			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libsharing_la_CFLAGS =			\
	-I$(top_srcdir)/plugins/common	\
	$(SHARING_CFLAGS)		\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

libsharing_la_LDFLAGS = $(CSD_PLUGIN_LDFLAGS)

libsharing_la_LIBADD  = $(SETTINGS_PLUGIN_LIBS) $(SHARING_LIBS)

noinst_PROGRAMS = csd-test-sharing
csd_test_sharing_SOURCES =				\
	csd-sharing-manager.c				\
	csd-sharing-manager.h				\
	test-sharing.c

csd_test_sharing_CFLAGS = $(libsharing_la_CFLAGS)
csd_test_sharing_CPPFLAGS = $(libsharing_la_CPPFLAGS)
csd_test_sharing_LDADD =				\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la \
	$(SHARING_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = sharing.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST = $(plugin_in_files)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@CSD_INTLTOOL_PLUGIN_RULE@
