libexec_PROGRAMS = cinnamon-fallback-mount-helper

cinnamon_fallback_mount_helper_SOURCES = \
	cinnamon-fallback-mount-helper.c \
	csd-automount-manager.c \
	csd-automount-manager.h \
	csd-autorun.c \
	csd-autorun.h

cinnamon_fallback_mount_helper_CPPFLAGS = \
	-I$(top_srcdir)/cinnamon-settings-daemon		\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	$(AM_CPPFLAGS)

cinnamon_fallback_mount_helper_CFLAGS =	\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(SYSTEMD_CFLAGS)		\
	$(AUTOMOUNT_CFLAGS)

cinnamon_fallback_mount_helper_LDADD =	\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(SYSTEMD_LIBS)			\
	$(AUTOMOUNT_LIBS)		\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la

autostartdir = $(datadir)/applications
autostart_in_files = cinnamon-fallback-mount-helper.desktop.in
autostart_in_in_files = cinnamon-fallback-mount-helper.desktop.in.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

$(autostart_in_files): $(autostart_in_in_files)
	@sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(autostart_in_in_files)

CLEANFILES = $(autostart_DATA) $(autostart_in_files)
