plugin_name = 'orientation'

orientation_sources = [
    'csd-orientation-manager.c',
    'main.c',
]

orientation_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-orientation',
    orientation_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: orientation_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)
if install_binfiles
    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-orientation')
endif

configure_file(
    input: 'cinnamon-settings-daemon-orientation.desktop.in',
    output: 'cinnamon-settings-daemon-orientation.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
