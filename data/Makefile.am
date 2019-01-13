NULL =

apidir = $(includedir)/cinnamon-settings-daemon-$(CSD_API_VERSION)/cinnamon-settings-daemon
api_DATA = csd-enums.h

gsettings_ENUM_NAMESPACE = org.cinnamon.settings-daemon
gsettings_ENUM_FILES = $(top_srcdir)/data/$(api_DATA)

gsettings_SCHEMAS =							\
	org.cinnamon.settings-daemon.peripherals.gschema.xml		\
	org.cinnamon.settings-daemon.peripherals.wacom.gschema.xml      \
	org.cinnamon.settings-daemon.plugins.gschema.xml			\
	org.cinnamon.settings-daemon.plugins.power.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.color.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.media-keys.gschema.xml	\
	org.cinnamon.settings-daemon.plugins.xsettings.gschema.xml		\
	org.cinnamon.settings-daemon.plugins.housekeeping.gschema.xml	\
	org.cinnamon.settings-daemon.plugins.xrandr.gschema.xml

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cinnamon-settings-daemon.pc

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST = 					\
	$(man_MANS)				\
	$(convert_DATA)				\
	$(gsettings_SCHEMAS:.xml=.xml.in.in)	\
	$(gsettings_ENUM_FILES)			\
	cinnamon-settings-daemon.pc.in		\
	$(api_DATA)				\
	$(dbusservice_in_files)			\
	$(NULL)

DISTCLEANFILES = 			\
	$(gsettings_SCHEMAS)		\
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in			\
	$(gsettings_SCHEMAS:.xml=.valid)
