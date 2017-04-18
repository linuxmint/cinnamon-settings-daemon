plugin_name = wacom

org.cinnamon.settings-daemon.plugins.wacom.policy.in: org.cinnamon.settings-daemon.plugins.wacom.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

csd-wacom-resources.c: wacom.gresource.xml tablet-layout.css
	glib-compile-resources 				\
		--target=$@ 				\
		--sourcedir=$(srcdir) 			\
		--generate-source 			\
		--c-name csd_wacom 			\
		$(srcdir)/wacom.gresource.xml

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.cinnamon.settings-daemon.plugins.wacom.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

# so it always gets included in the tarball
csd_wacom_led_helper_SOURCES =	csd-wacom-led-helper.c

EXTRA_DIST = $(csd_wacom_led_helper_SOURCES) wacom.gresource.xml tablet-layout.css

if HAVE_GUDEV
libexec_PROGRAMS = csd-wacom-led-helper

csd_wacom_led_helper_LDFLAGS =				\
	$(WARN_LDFLAGS)					\
	$(BACKLIGHT_HELPER_LIBS)			\
	-lm

csd_wacom_led_helper_CFLAGS =				\
	$(WARN_CFLAGS)					\
	$(BACKLIGHT_HELPER_CFLAGS)
else
libexec_PROGRAMS =
endif

EXTRA_DIST += org.cinnamon.settings-daemon.plugins.wacom.policy.in.in

libexec_PROGRAMS += csd-wacom csd-list-wacom csd-wacom-osd

csd_wacom_SOURCES =	\
	main.c		\
	csd-wacom-manager.c	\
	csd-wacom-manager.h	\
	csd-wacom-osd-window.h	\
	csd-wacom-osd-window.c	\
	csd-wacom-device.c	\
	csd-wacom-device.h	\
	csd-wacom-resources.c

csd_wacom_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_wacom_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

csd_wacom_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

csd_list_wacom_SOURCES =	\
	list-wacom.c		\
	csd-wacom-device.c	\
	csd-wacom-device.h

csd_list_wacom_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

csd_list_wacom_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

csd_list_wacom_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

csd_wacom_osd_SOURCES =					\
	test-osd-window.c					\
	csd-wacom-osd-window.h					\
	csd-wacom-osd-window.c					\
	csd-wacom-device.c					\
	csd-wacom-device.h					\
	csd-wacom-resources.c

csd_wacom_osd_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_wacom_osd_CFLAGS =					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(WACOM_CFLAGS)						\
	$(AM_CFLAGS)

csd_wacom_osd_LDADD = \
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-wacom.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-wacom.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST += README.config-storage $(desktop_in_files)

CLEANFILES =						\
	$(desktop_DATA)					\
	csd-wacom-resources.c				\
	org.cinnamon.settings-daemon.plugins.wacom.policy	\
	org.cinnamon.settings-daemon.plugins.wacom.policy.in
