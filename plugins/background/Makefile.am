NULL =

plugin_name = background

libexec_PROGRAMS = csd-test-background

csd_test_background_SOURCES =       \
    test-background.c       \
    csd-background-manager.h    \
    csd-background-manager.c    \
    $(NULL)

csd_test_background_CPPFLAGS =              \
    -I$(top_srcdir)/cinnamon-settings-daemon        \
    -I$(top_srcdir)/plugins/common          \
    -DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
    $(AM_CPPFLAGS)

csd_test_background_CFLAGS = \
    $(PLUGIN_CFLAGS)        \
    $(SETTINGS_PLUGIN_CFLAGS)   \
    $(BACKGROUND_CFLAGS)        \
    $(AM_CFLAGS)

csd_test_background_LDADD =             \
    $(top_builddir)/cinnamon-settings-daemon/libcsd.la  \
    $(SETTINGS_PLUGIN_LIBS)             \
    $(BACKGROUND_LIBS)              \
    $(NULL)

plugin_LTLIBRARIES = \
    libbackground.la        \
    $(NULL)

libbackground_la_SOURCES =      \
    csd-background-plugin.h     \
    csd-background-plugin.c     \
    csd-background-manager.h    \
    csd-background-manager.c    \
    $(NULL)

libbackground_la_CPPFLAGS = \
    -I$(top_srcdir)/cinnamon-settings-daemon        \
    -I$(top_srcdir)/plugins/background/libbackground   \
    -DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
    $(AM_CPPFLAGS)

libbackground_la_CFLAGS = \
    $(SETTINGS_PLUGIN_CFLAGS)   \
    $(BACKGROUND_CFLAGS)        \
    $(AM_CFLAGS)

libbackground_la_LDFLAGS =      \
    $(CSD_PLUGIN_LDFLAGS)       \
    $(NULL)

libbackground_la_LIBADD  =      \
    $(SETTINGS_PLUGIN_LIBS)     \
    $(BACKGROUND_LIBS)      \
    $(NULL)

plugin_in_files =       \
    background.cinnamon-settings-plugin.in  \
    $(NULL)

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST =            \
    $(plugin_in_files)  \
    $(NULL)

CLEANFILES =            \
    $(plugin_DATA)      \
    $(NULL)

DISTCLEANFILES =        \
    $(plugin_DATA)      \
    $(NULL)

@CSD_INTLTOOL_PLUGIN_RULE@