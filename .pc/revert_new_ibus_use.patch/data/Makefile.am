NULL =

apidir = $(includedir)/gnome-settings-daemon-$(GSD_API_VERSION)/gnome-settings-daemon
api_DATA = gsd-enums.h

gsettings_ENUM_NAMESPACE = org.gnome.settings-daemon
gsettings_ENUM_FILES = $(top_srcdir)/data/$(api_DATA)

gsettings_SCHEMAS =							\
	org.gnome.settings-daemon.peripherals.gschema.xml		\
	org.gnome.settings-daemon.plugins.gschema.xml			\
	org.gnome.settings-daemon.plugins.keyboard.gschema.xml		\
	org.gnome.settings-daemon.plugins.power.gschema.xml		\
	org.gnome.settings-daemon.plugins.color.gschema.xml		\
	org.gnome.settings-daemon.plugins.media-keys.gschema.xml	\
	org.gnome.settings-daemon.plugins.xsettings.gschema.xml		\
	org.gnome.settings-daemon.plugins.housekeeping.gschema.xml	\
	org.gnome.settings-daemon.plugins.print-notifications.gschema.xml	\
	org.gnome.settings-daemon.plugins.xrandr.gschema.xml		\
	org.gnome.settings-daemon.plugins.updates.gschema.xml		\
	org.gnome.settings-daemon.plugins.orientation.gschema.xml	\
	org.gnome.settings-daemon.peripherals.wacom.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

convertdir = $(datadir)/GConf/gsettings
convert_DATA = gnome-settings-daemon.convert

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = gnome-settings-daemon.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

gnome-settings-daemon.desktop.in: gnome-settings-daemon.desktop.in.in
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@.tmp && mv $@.tmp $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-settings-daemon.pc

@INTLTOOL_XML_NOMERGE_RULE@

man_MANS = gnome-settings-daemon.1

dbusservice_in_files = org.freedesktop.IBus.service.in

EXTRA_DIST = 					\
	$(man_MANS)				\
	$(convert_DATA)				\
	$(gsettings_SCHEMAS:.xml=.xml.in.in)	\
	$(desktop_in_files)			\
	$(gsettings_ENUM_FILES)			\
	gnome-settings-daemon.pc.in		\
	$(api_DATA)				\
	$(dbusservice_in_files)			\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)		\
	$(desktop_DATA)			\
	gnome-settings-daemon.desktop.in \
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in			\
	$(gsettings_SCHEMAS:.xml=.valid)

if HAVE_IBUS
dbusservicedir=${datadir}/dbus-1/services
dbusservice_DATA = $(dbusservice_in_files:.service.in=.service)

org.freedesktop.IBus.service: org.freedesktop.IBus.service.in
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@.tmp && mv $@.tmp $@

CLEANFILES = $(dbusservice_DATA)
endif
