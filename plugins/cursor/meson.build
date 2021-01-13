plugin_name = 'cursor'

cursor_sources = [
    'csd-cursor-manager.c',
    'main.c',
]

cursor_deps = [
    common_dep,
    csd_dep,
    libnotify,
    xfixes,
]

executable(
    'csd-cursor',
    cursor_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: cursor_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-cursor')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-cursor')
endif

configure_file(
    input: 'cinnamon-settings-daemon-cursor.desktop.in',
    output: 'cinnamon-settings-daemon-cursor.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
