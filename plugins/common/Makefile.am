plugin_name = common

noinst_LTLIBRARIES = libcommon.la

AM_CFLAGS = $(WARN_CFLAGS)

libcommon_la_SOURCES = \
	csd-keygrab.c		\
	csd-keygrab.h		\
	csd-input-helper.c	\
	csd-input-helper.h	\
	csd-power-helper.c	\
	csd-power-helper.h

libcommon_la_CPPFLAGS = \
	$(AM_CPPFLAGS)

libcommon_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(COMMON_CFLAGS)		\
	$(AM_CFLAGS)

libcommon_la_LDFLAGS = \
	$(WARN_LDFLAGS)		\
	$(CSD_PLUGIN_LDFLAGS)

libcommon_la_LIBADD  = \
	$(SETTINGS_PLUGIN_LIBS)		\
	$(COMMON_LIBS)

libexec_PROGRAMS = csd-test-input-helper

csd_test_input_helper_SOURCES = test-input-helper.c
csd_test_input_helper_LDADD = libcommon.la
csd_test_input_helper_CFLAGS = $(libcommon_la_CFLAGS)

noinst_PROGRAMS = test-egg-key-parsing

test_egg_key_parsing_SOURCES = test-egg-key-parsing.c
test_egg_key_parsing_LDADD = libcommon.la $(COMMON_LIBS)
test_egg_key_parsing_CFLAGS = $(libcommon_la_CFLAGS)

scriptsdir = $(datadir)/cinnamon-settings-daemon-@CSD_API_VERSION@
scripts_DATA = input-device-example.sh

EXTRA_DIST = $(scripts_DATA) test-plugin.h
