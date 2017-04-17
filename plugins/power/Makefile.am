plugin_name = power

AM_CFLAGS = $(WARN_CFLAGS)

org.cinnamon.settings-daemon.plugins.power.policy.in: org.cinnamon.settings-daemon.plugins.power.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.cinnamon.settings-daemon.plugins.power.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

# so it always gets included in the tarball
csd_backlight_helper_SOURCES =				\
	csd-backlight-helper.c

libexec_PROGRAMS = csd-power
csd_power_SOURCES =				\
	gpm-common.c					\
	gpm-common.h					\
	gpm-phone.c					\
	gpm-phone.h					\
	gpm-idletime.c					\
	gpm-idletime.h					\
	csd-power-manager.c				\
	csd-power-manager.h				\
	main.c

csd_power_CFLAGS = \
	$(PLUGIN_CFLAGS)				\
	$(POWER_CFLAGS)					\
	$(SETTINGS_PLUGIN_CFLAGS)			\
	$(AM_CFLAGS)

csd_power_CPPFLAGS = \
	-I$(top_srcdir)/data/				\
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"		\
	-DSBINDIR=\"$(sbindir)\"			\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	$(AM_CPPFLAGS)

csd_power_LDADD =					\
	-lm						\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la \
	$(POWER_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-power.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-power.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = 						\
	$(desktop_in_files)				\
	$(csd_backlight_helper_SOURCES)

if HAVE_GUDEV
libexec_PROGRAMS +=					\
	csd-backlight-helper

csd_backlight_helper_LDFLAGS =				\
	$(BACKLIGHT_HELPER_LIBS)			\
	-lm

csd_backlight_helper_CFLAGS =				\
	$(WARN_CFLAGS)					\
	$(BACKLIGHT_HELPER_CFLAGS)

EXTRA_DIST += 						\
	org.cinnamon.settings-daemon.plugins.power.policy.in.in
endif

clean-local:
	rm -f *~

CLEANFILES = 						\
	$(desktop_DATA)					\
	org.cinnamon.settings-daemon.plugins.power.policy	\
	org.cinnamon.settings-daemon.plugins.power.policy.in
