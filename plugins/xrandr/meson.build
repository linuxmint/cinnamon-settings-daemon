plugin_name = 'xrandr'

xrandr_sources = [
    'csd-xrandr-manager.c',
    'main.c',
]

xrandr_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    libnotify,
    upower_glib,
    xfixes,
]

executable(
    'csd-xrandr',
    xrandr_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: xrandr_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

configure_file(
    input: 'cinnamon-settings-daemon-xrandr.desktop.in',
    output: 'cinnamon-settings-daemon-xrandr.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
