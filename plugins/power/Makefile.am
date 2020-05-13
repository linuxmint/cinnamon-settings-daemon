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

csd-power-proxy.h : Makefile.am org.cinnamon.SettingsDaemon.Power.xml
	gdbus-codegen                                       \
		--c-namespace Csd                      \
		--annotate "org.cinnamon.SettingsDaemon.Power" org.gtk.GDBus.C.Name "Power"    \
		--generate-c-code csd-power-proxy \
		org.cinnamon.SettingsDaemon.Power.xml                          \
		$(NULL)
csd-power-proxy.c : csd-power-proxy.h

csd-power-screen-proxy.h : Makefile.am org.cinnamon.SettingsDaemon.Power.Screen.xml
	gdbus-codegen                                       \
		--c-namespace Csd                      \
		--annotate "org.cinnamon.SettingsDaemon.Power.Screen" org.gtk.GDBus.C.Name "Screen"    \
		--generate-c-code csd-power-screen-proxy \
		org.cinnamon.SettingsDaemon.Power.Screen.xml                          \
		$(NULL)
csd-power-screen-proxy.c : csd-power-screen-proxy.h

csd-power-keyboard-proxy.h : Makefile.am org.cinnamon.SettingsDaemon.Power.Keyboard.xml
	gdbus-codegen                                       \
		--c-namespace Csd                      \
		--annotate "org.cinnamon.SettingsDaemon.Power.Keyboard" org.gtk.GDBus.C.Name "Keyboard"    \
		--generate-c-code csd-power-keyboard-proxy \
		org.cinnamon.SettingsDaemon.Power.Keyboard.xml                          \
		$(NULL)
csd-power-keyboard-proxy.c : csd-power-keyboard-proxy.h

csd-power-manager.c : csd-power-proxy.h csd-power-screen-proxy.h csd-power-keyboard-proxy.h

libexec_PROGRAMS = csd-power
csd_power_SOURCES =				\
	gpm-common.c					\
	gpm-common.h					\
	gpm-phone.c					\
	gpm-phone.h					\
	gpm-idletime.c					\
	gpm-idletime.h					\
	csd-power-proxy.c \
	csd-power-proxy.h  \
	csd-power-screen-proxy.c \
	csd-power-screen-proxy.h \
	csd-power-keyboard-proxy.c \
	csd-power-keyboard-proxy.h \
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
