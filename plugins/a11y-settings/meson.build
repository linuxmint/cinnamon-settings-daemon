plugin_name = 'a11y-settings'

a11y_settings_sources = [
    'csd-a11y-settings-manager.c',
    'main.c',
]

a11y_settings_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-a11y-settings',
    a11y_settings_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: a11y_settings_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: pkglibdir,
)

configure_file(
    input: 'cinnamon-settings-daemon-a11y-settings.desktop.in',
    output: 'cinnamon-settings-daemon-a11y-settings.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
