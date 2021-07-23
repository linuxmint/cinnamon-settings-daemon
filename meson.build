project('cinnamon-settings-daemon', 'c', version: '5.0.4', meson_version: '>= 0.47')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

version = meson.project_version()
pkgname = meson.project_name().to_lower()
api_version = '3.0'

cc = meson.get_compiler('c')
cargs = []

# directories
prefix = get_option('prefix')
bindir = get_option('bindir')
datadir = get_option('datadir')
libdir = get_option('libdir')
libexecdir = get_option('libexecdir')
includedir = get_option('includedir')
desktopdir = join_paths(datadir, 'applications')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
pkglibdir = join_paths(libdir, pkgname)
apilibdir = join_paths(libdir, '@0@-@1@'.format(pkgname, api_version))
pkgdatadir = join_paths(datadir, pkgname)
gtkbuilderdir = join_paths(prefix, datadir, pkgname)
pkgincludedir = join_paths(includedir, pkgname)
localedir = join_paths(prefix, datadir, 'locale')
polkitdir = join_paths(datadir, 'polkit-1', 'actions')
sysconfdir = get_option('sysconfdir')
autostartdir = join_paths(sysconfdir, 'xdg', 'autostart')

dbusservicedir = get_option('dbus_service_dir')
if dbusservicedir == ''
    dbusservicedir = join_paths(datadir, 'dbus-1', 'system-services')
endif

dbussystemdir = get_option('dbus_system_dir')
if dbussystemdir == ''
    dbussystemdir = join_paths(datadir, 'dbus-1', 'system.d')
endif

# dependencies
cinnamon_desktop_required = '>= 4.8.0'
canberra = dependency('libcanberra-gtk3')
cinnamon_desktop = dependency('cinnamon-desktop', version: cinnamon_desktop_required)
colord = dependency('colord', version: '>= 0.1.27', required: get_option('use_color'))
cups = dependency('cups', version: '>= 1.4', required: get_option('use_cups'))
cvc = dependency('cvc', version: cinnamon_desktop_required)
fontconfig = dependency('fontconfig')
gio = dependency('gio-2.0', version: '>= 2.40.0')
glib = dependency('glib-2.0', version: '>= 2.40.0')
gnomekbd_required = '>= 3.6.0'
gnomekbd = dependency('libgnomekbd', version: gnomekbd_required)
gnomekbdui = dependency('libgnomekbdui', version: gnomekbd_required)
gtk = dependency('gtk+-3.0', version: '>= 3.14.0')
gudev = dependency('gudev-1.0', required: get_option('use_gudev'))
libnotify = dependency('libnotify', version: '>= 0.7.3')
kbproto = dependency('kbproto')
nss = dependency('nss', version: '>= 3.11.2', required: get_option('use_smartcard'))
polkit = dependency('polkit-gobject-1', version: '>= 0.97', required: get_option('use_polkit'))
pulse_required = '>= 0.9.16'
pulse = dependency('libpulse', version: pulse_required)
pulse_glib = dependency('libpulse-mainloop-glib', version: pulse_required)
upower_glib = dependency('upower-glib', version: '>= 0.9.11')
wacom = dependency('libwacom', version: '>= 0.7', required: get_option('use_wacom'))
x11 = dependency('x11')
xext = dependency('xext')
xfixes = dependency('xfixes')
xi = dependency('xi')
xklavier = dependency('libxklavier', version: '>= 5.0')
xtst = dependency('xtst')

# currently we only use dbus if we're also using polkit if this changes in the future we can
# simply remove the required field here
dbus = dependency('dbus-1', version: '>= 1.1.2', required: polkit.found())
dbus_glib = dependency('dbus-glib-1', required: polkit.found())

# currently only used for the wacom plugin
librsvg = dependency('librsvg-2.0', version: '>= 2.36.2', required: wacom.found())
xorg_wacom = dependency('xorg-wacom', required: wacom.found())

lcms = dependency('lcms2', required: colord.found())
if lcms.version().version_compare('>=2.2')
    cargs += '-DHAVE_NEW_LCMS'
endif

using_logind = false
if not get_option('use_logind').disabled()
    logind = dependency('libsystemd-logind', required: false)
    if not logind.found()
        logind = dependency('libsystemd', required: false)
    endif
    if not logind.found()
        # if logind is explicitly enabled, we want to make sure it gives an error if we don't find anything
        logind = dependency('libelogind', required: get_option('use_logind'))
    endif
    if logind.found()
        cargs += '-DHAVE_LOGIND'
        using_logind = true
    endif
endif

cc = meson.get_compiler('c')
math = cc.find_library('m', required: false)

# currently used by the datetime plugin, but dbus-glib is deprecated so we
# should port it to gdbus asap, and this should be removed at that point
dbus_binding_tool = find_program('dbus-binding-tool')

csd_conf = configuration_data()
csd_conf.set_quoted('GTKBUILDERDIR', gtkbuilderdir)
csd_conf.set_quoted('CINNAMON_SETTINGS_LOCALEDIR', localedir)
csd_conf.set_quoted('PACKAGE', meson.project_name())
csd_conf.set_quoted('PACKAGE_NAME', meson.project_name())
csd_conf.set_quoted('PACKAGE_VERSION', meson.project_version())
csd_conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())
csd_conf.set_quoted('LIBEXECDIR', join_paths(prefix, libexecdir))
csd_conf.set_quoted('SYSCONFDIR', sysconfdir)
csd_conf.set_quoted('LIBDIR', libdir)

if gudev.found()
    cargs += '-DHAVE_GUDEV'
endif

if wacom.found()
    cargs += '-DHAVE_WACOM'
endif

if not get_option('enable_debug')
    cargs += [
          '-Wno-deprecated-declarations',
          '-Wno-deprecated',
          '-Wno-declaration-after-statement',
          '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    ]
endif

add_global_arguments(
    cargs,
    language: 'c'
)

# generate config.h
config_h_file = configure_file(
    output : 'config.h',
    configuration : csd_conf
)

config_h = declare_dependency(
    sources: config_h_file
)

include_dirs = include_directories('.', 'cinnamon-settings-daemon')

subdir('data')
subdir('cinnamon-settings-daemon')
subdir('plugins')

install_subdir(
    'files',
    install_dir: '/',
    strip_directory: true,
)

subdir('install-scripts')
