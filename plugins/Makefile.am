NULL =

enabled_plugins =	\
	a11y-keyboard	\
	a11y-settings	\
	automount	\
	background  \
	clipboard	\
	color		\
	cursor		\
	dummy		\
    datetime    \
	power		\
	housekeeping    \
	keyboard	\
	media-keys	\
	mouse		\
	orientation	\
	screensaver-proxy \
	sound		\
	xrandr		\
	xsettings	\
	$(NULL)

disabled_plugins = $(NULL)

if BUILD_WACOM
enabled_plugins += wacom
else
disabled_plugins += wacom
endif

if SMARTCARD_SUPPORT
enabled_plugins += smartcard
else
disabled_plugins += smartcard
endif

if BUILD_PRINT_NOTIFICATIONS
enabled_plugins += print-notifications
else
disabled_plugins += print-notifications
endif

SUBDIRS = common $(enabled_plugins)
DIST_SUBDIRS = $(SUBDIRS) $(disabled_plugins)
