plugin_name = 'xsettings'

xsettings_common_sources = [
    'csd-xsettings-gtk.c',
]

xsettings_sources = [
    'csd-xsettings-manager.c',
    'xsettings-common.c',
    'xsettings-manager.c',
    'fontconfig-monitor.c',
    'main.c',
    xsettings_common_sources,
]

test_xsettings_sources = [
    'test-gtk-modules.c',
    xsettings_common_sources,
]

xsettings_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    fontconfig,
    libnotify,
]

executable(
    'csd-xsettings',
    xsettings_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: xsettings_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
        '-DGTK_MODULES_DIRECTORY="@0@/@1@/cinnamon-settings-daemon-@2@/gtk-modules/"'.format(prefix, libdir, api_version),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-xsettings')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-xsettings')
endif

executable(
    'test-gtk-modules',
    test_xsettings_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: xsettings_deps,
    c_args: [
        '-DGTK_MODULES_DIRECTORY="@0@/@1@/cinnamon-settings-daemon-@2@/gtk-modules/"'.format(prefix, libdir, api_version),
    ],
)

configure_file(
    input: 'cinnamon-settings-daemon-xsettings.desktop.in',
    output: 'cinnamon-settings-daemon-xsettings.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
