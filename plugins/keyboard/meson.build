plugin_name = 'keyboard'

keyboard_sources = [
    'csd-keyboard-manager.c',
    'main.c',
]

keyboard_deps = [
    common_dep,
    csd_dep,
]

executable(
    'csd-keyboard',
    keyboard_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: keyboard_deps,
    c_args: [
        '-DG_LOG_DOMAIN="csd-@0@"'.format(plugin_name),
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
        '-DDATADIR="@0@"'.format(datadir),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-keyboard')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-keyboard')
endif

configure_file(
    input: 'cinnamon-settings-daemon-keyboard.desktop.in',
    output: 'cinnamon-settings-daemon-keyboard.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
