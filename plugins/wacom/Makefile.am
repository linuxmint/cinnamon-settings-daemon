plugin_name = wacom

plugin_LTLIBRARIES = libcsdwacom.la

libcsdwacom_la_SOURCES =	\
	csd-wacom-plugin.c	\
	csd-wacom-manager.h	\
	csd-wacom-manager.c	\
	csd-wacom-osd-window.h	\
	csd-wacom-osd-window.c	\
	csd-wacom-device.c	\
	csd-wacom-device.h	\
	csd-wacom-resources.c

libcsdwacom_la_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-I$(top_srcdir)/data/				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

libcsdwacom_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

libcsdwacom_la_LDFLAGS =		\
	$(CSD_PLUGIN_LDFLAGS)

libcsdwacom_la_LIBADD  =						\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(SETTINGS_PLUGIN_LIBS)						\
	$(WACOM_LIBS)

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
	$(BACKLIGHT_HELPER_LIBS)			\
	-lm

csd_wacom_led_helper_CFLAGS =				\
	$(BACKLIGHT_HELPER_CFLAGS)
else
libexec_PROGRAMS =
endif

EXTRA_DIST += org.cinnamon.settings-daemon.plugins.wacom.policy.in.in

libexec_PROGRAMS += csd-test-wacom csd-list-wacom csd-test-wacom-osd

csd_test_wacom_SOURCES =	\
	test-wacom.c		\
	csd-wacom-manager.c	\
	csd-wacom-manager.h	\
	csd-wacom-osd-window.h	\
	csd-wacom-osd-window.c	\
	csd-wacom-device.c	\
	csd-wacom-device.h	\
	csd-wacom-resources.c

csd_test_wacom_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DSCHEMA_NAME=\""csdwacom"\"				\
	$(AM_CPPFLAGS)

csd_test_wacom_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

csd_test_wacom_LDADD =						\
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

csd_test_wacom_osd_SOURCES =					\
	test-osd-window.c					\
	csd-wacom-osd-window.h					\
	csd-wacom-osd-window.c					\
	csd-wacom-device.c					\
	csd-wacom-device.h					\
	csd-wacom-resources.c

csd_test_wacom_osd_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_test_wacom_osd_CFLAGS =					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(WACOM_CFLAGS)						\
	$(AM_CFLAGS)

csd_test_wacom_osd_LDADD = \
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

plugin_in_files = wacom.cinnamon-settings-plugin.in

plugin_DATA = $(plugin_in_files:.cinnamon-settings-plugin.in=.cinnamon-settings-plugin)

EXTRA_DIST += $(plugin_in_files) README.config-storage
CLEANFILES =						\
	$(plugin_DATA)					\
	csd-wacom-resources.c				\
	org.cinnamon.settings-daemon.plugins.wacom.policy	\
	org.cinnamon.settings-daemon.plugins.wacom.policy.in

@CSD_INTLTOOL_PLUGIN_RULE@
