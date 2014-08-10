NULL =

apidir = $(includedir)/cinnamon-settings-daemon-$(CSD_API_VERSION)/cinnamon-settings-daemon
api_DATA = csd-enums.h

gsettings_ENUM_NAMESPACE = org.cinnamon.settings-daemon
gsettings_ENUM_FILES = $(top_srcdir)/data/$(api_DATA)

gsettings_SCHEMAS =							\
	org.cinnamon.settings-daemon.peripherals.gschema.xml		\
	org.cinnamon.settings-daemon.peripherals.wacom.gschema.xml      \
	org.cinnamon.settings-daemon.plugins.gschema.xml			\
	org.cinnamon.settings-daemon.plugins.keyboard.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.power.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.color.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.media-keys.gschema.xml	\
	org.cinnamon.settings-daemon.plugins.xsettings.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.housekeeping.gschema.xml	\
	org.cinnamon.settings-daemon.plugins.print-notifications.gschema.xml	\
	org.cinnamon.settings-daemon.plugins.xrandr.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.orientation.gschema.xml   \
	org.cinnamon.settings-daemon.plugins.background.gschema.xml
	
@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = cinnamon-settings-daemon.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

cinnamon-settings-daemon.desktop.in: cinnamon-settings-daemon.desktop.in.in
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@.tmp && mv $@.tmp $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cinnamon-settings-daemon.pc

@INTLTOOL_XML_NOMERGE_RULE@

man_MANS = cinnamon-settings-daemon.1

dbusservice_in_files = org.freedesktop.IBus.service.in

EXTRA_DIST = 					\
	$(man_MANS)				\
	$(convert_DATA)				\
	$(gsettings_SCHEMAS:.xml=.xml.in.in)	\
	$(desktop_in_files)			\
	$(gsettings_ENUM_FILES)			\
	cinnamon-settings-daemon.pc.in		\
	$(api_DATA)				\
	$(dbusservice_in_files)			\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)		\
	$(desktop_DATA)			\
	cinnamon-settings-daemon.desktop.in \
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in			\
	$(gsettings_SCHEMAS:.xml=.valid)

