NULL =

enabled_plugins =	\
	a11y-keyboard	\
	a11y-settings	\
	automount	\
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
	screensaver-proxy \
	sound		\
	xrandr		\
	xsettings	\
	$(NULL)

disabled_plugins = $(NULL)

if SMARTCARD_SUPPORT
enabled_plugins += smartcard
else
disabled_plugins += smartcard
endif

if HAVE_GUDEV
enabled_plugins += orientation
else
disabled_plugins += orientation
endif

if BUILD_PRINT_NOTIFICATIONS
enabled_plugins += print-notifications
else
disabled_plugins += print-notifications
endif

SUBDIRS = common $(enabled_plugins)
DIST_SUBDIRS = $(SUBDIRS) $(disabled_plugins)
