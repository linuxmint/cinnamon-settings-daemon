plugin_name = mouse

AM_CFLAGS = $(WARN_CFLAGS)

libexec_PROGRAMS = csd-locate-pointer

csd_locate_pointer_SOURCES = 	\
	csd-locate-pointer.h	\
	csd-locate-pointer.c	\
	csd-timeline.h		\
	csd-timeline.c

csd_locate_pointer_CFLAGS = \
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)			\
	$(AM_CFLAGS)

csd_locate_pointer_LDADD  = 		\
	$(SETTINGS_PLUGIN_LIBS)		\
	$(MOUSE_LIBS)			\
	-lm

libexec_PROGRAMS += csd-mouse

csd_mouse_SOURCES =		\
	main.c			\
	csd-mouse-manager.c		\
	csd-mouse-manager.h

csd_mouse_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/cinnamon-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DCINNAMON_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

csd_mouse_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MOUSE_CFLAGS)		\
	$(AM_CFLAGS)

csd_mouse_LDADD =						\
	$(top_builddir)/cinnamon-settings-daemon/libcsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MOUSE_LIBS)			\
	-lm

desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = cinnamon-settings-daemon-mouse.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

cinnamon-settings-daemon-mouse.desktop: $(desktop_in_files) Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

DISTCLEANFILES = \
	$(desktop_DATA)
