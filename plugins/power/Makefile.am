plugin_name = power

plugin_LTLIBRARIES =					\
	libpower.la

libpower_la_SOURCES = 					\
	gpm-common.c					\
	gpm-common.h					\
	gpm-phone.c					\
	gpm-phone.h					\
	gpm-idletime.c					\
	gpm-idletime.h					\
	csd-power-manager.c				\
	csd-power-manager.h				\
	csd-power-plugin.c				\
	csd-power-plugin.h

libpower_la_CPPFLAGS = \
	-I$(top_srcdir)/data/				\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"		\
	-DSBINDIR=\"$(sbindir)\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	$(AM_CPPFLAGS)

libpower_la_CFLAGS = \
	$(PLUGIN_CFLAGS)				\
	$(POWER_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)			\
	$(AM_CFLAGS)

libpower_la_LDFLAGS = 					\
	$(CSD_PLUGIN_LDFLAGS)

libpower_la_LIBADD  = 					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la \
	$(POWER_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files = 					\
	power.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

org.cinnamon.settings-daemon.plugins.power.policy.in: org.cinnamon.settings-daemon.plugins.power.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.cinnamon.settings-daemon.plugins.power.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

# so it always gets included in the tarball
csd_backlight_helper_SOURCES =				\
	csd-backlight-helper.c

libexec_PROGRAMS = csd-test-power
csd_test_power_SOURCES =				\
	gpm-common.c					\
	gpm-common.h					\
	gpm-phone.c					\
	gpm-phone.h					\
	gpm-idletime.c					\
	gpm-idletime.h					\
	csd-power-manager.c				\
	csd-power-manager.h				\
	test-power.c

csd_test_power_CFLAGS = $(libpower_la_CFLAGS)
csd_test_power_CPPFLAGS = $(libpower_la_CPPFLAGS)
csd_test_power_LDADD =					\
	-lm						\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la \
	$(POWER_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

EXTRA_DIST = 						\
	$(plugin_in_files)				\
	$(csd_backlight_helper_SOURCES)

if HAVE_GUDEV
libexec_PROGRAMS +=					\
	csd-backlight-helper

csd_backlight_helper_LDFLAGS =				\
	$(BACKLIGHT_HELPER_LIBS)			\
	-lm

csd_backlight_helper_CFLAGS =				\
	$(BACKLIGHT_HELPER_CFLAGS)

EXTRA_DIST += 						\
	org.cinnamon.settings-daemon.plugins.power.policy.in.in
endif

clean-local:
	rm -f *~

CLEANFILES = 						\
	$(plugin_DATA)					\
	org.cinnamon.settings-daemon.plugins.power.policy	\
	org.cinnamon.settings-daemon.plugins.power.policy.in

@CSD_INTLTOOL_PLUGIN_RULE@
