plugin_name = dummy

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-dummy
csd_dummy_SOURCES =		\
	main.c			\
	csd-dummy-manager.c		\
	csd-dummy-manager.h

csd_dummy_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(AM_CFLAGS)

csd_dummy_CPPFLAGS =		\
	-I$(top_srcdir)/plugins/common	\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

csd_dummy_LDADD = $(SETTINGS_PLUGIN_LIBS) $(top_builddir)/cinnamon-settings-daemon/libcsd.la
