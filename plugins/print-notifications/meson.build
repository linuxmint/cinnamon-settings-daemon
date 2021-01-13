plugin_name = 'print-notifications'

print_notifications_sources = [
    'csd-print-notifications-manager.c',
    'main.c',
]

printer_sources = [
    'csd-printer.c',
]

print_notifications_deps = [
    common_dep,
    config_h,
    csd_dep,
    cups,
    libnotify,
]

executable(
    'csd-print-notifications',
    print_notifications_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: print_notifications_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-print-notifications')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-print-notifications')
endif

executable(
    'csd-printer',
    printer_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: print_notifications_deps,
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-printer')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-printer')
endif

configure_file(
    input: 'cinnamon-settings-daemon-print-notifications.desktop.in',
    output: 'cinnamon-settings-daemon-print-notifications.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
