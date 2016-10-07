plugin_name = datetime

dbus_servicesdir = $(datadir)/dbus-1/system-services
dbus_confdir = $(sysconfdir)/dbus-1/system.d
polkitdir = $(datadir)/polkit-1/actions

dbus_services_in_files = org.cinnamon.SettingsDaemon.DateTimeMechanism.service.in
polkit_in_files = org.cinnamon.settingsdaemon.datetimemechanism.policy.in

csd-datetime-mechanism-glue.h: $(srcdir)/csd-datetime-mechanism.xml
	$(AM_V_GEN) dbus-binding-tool \
			--prefix=csd_datetime_mechanism --mode=glib-server	\
			--output=csd-datetime-mechanism-glue.h			\
			$(srcdir)/csd-datetime-mechanism.xml


if HAVE_POLKIT
libexec_PROGRAMS = csd-datetime-mechanism
noinst_PROGRAMS = test-system-timezone
endif

csd_datetime_mechanism_SOURCES =		\
	csd-datetime-mechanism.c		\
	csd-datetime-mechanism.h		\
	csd-datetime-mechanism-fedora.c		\
	csd-datetime-mechanism-fedora.h		\
	csd-datetime-mechanism-debian.c		\
	csd-datetime-mechanism-debian.h		\
	csd-datetime-mechanism-suse.c		\
	csd-datetime-mechanism-suse.h		\
	csd-datetime-mechanism-main.c		\
	system-timezone.c			\
	system-timezone.h


if HAVE_POLKIT
BUILT_SOURCES = csd-datetime-mechanism-glue.h
endif

AM_CFLAGS = $(WARN_CFLAGS) $(PLUGIN_CFLAGS) $(SETTINGS_PLUGIN_CFLAGS) $(POLKIT_CFLAGS)
csd_datetime_mechanism_LDADD = $(POLKIT_LIBS) $(SETTINGS_PLUGIN_LIBS)

test_system_timezone_SOURCES = test-system-timezone.c system-timezone.c system-timezone.h
test_system_timezone_LDADD = $(POLKIT_LIBS) $(SETTINGS_PLUGIN_LIBS)

if HAVE_POLKIT
dbus_services_DATA = $(dbus_services_in_files:.service.in=.service)

$(dbus_services_DATA): $(dbus_services_in_files)
	$(AM_V_GEN)sed -e "s|\@LIBEXECDIR\@|$(libexecdir)|" $< > $@

dbus_conf_DATA = org.cinnamon.SettingsDaemon.DateTimeMechanism.conf

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

else
dbus_services_DATA =
dbus_conf_DATA =
polkit_DATA =
endif

EXTRA_DIST =						\
	$(dbus_services_in_files)			\
	org.cinnamon.SettingsDaemon.DateTimeMechanism.conf	\
	$(polkit_in_files)				\
	csd-datetime-mechanism.xml

CLEANFILES = 		\
	org.cinnamon.SettingsDaemon.DateTimeMechanism.service	\
	org.cinnamon.settingsdaemon.datetimemechanism.policy	\
	$(BUILT_SOURCES)
