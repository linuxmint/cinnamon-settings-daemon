plugin_name = 'mouse'

mouse_sources = [
    'csd-mouse-manager.c',
    'main.c',
]

find_pointer_sources = [
    'csd-locate-pointer.c',
    'csd-timeline.c',
]

mouse_deps = [
    common_dep,
    csd_dep,
    libnotify,
    math,
]

executable(
    'csd-mouse',
    mouse_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: mouse_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

executable(
    'csd-locate-pointer',
    find_pointer_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: mouse_deps,
    install: true,
    install_dir: libexecdir,
)

configure_file(
    input: 'cinnamon-settings-daemon-mouse.desktop.in',
    output: 'cinnamon-settings-daemon-mouse.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
