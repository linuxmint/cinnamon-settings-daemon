plugin_name = 'keyboard'

keyboard_sources = [
    'csd-keyboard-manager.c',
    'csd-keyboard-xkb.c',
    'delayed-dialog.c',
    'gkbd-configuration.c',
    'main.c',
]

keyboard_deps = [
    common_dep,
    csd_dep,
    gnomekbd,
    gnomekbdui,
    libnotify,
    xklavier,
]

executable(
    'csd-keyboard',
    keyboard_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: keyboard_deps,
    c_args: [
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

keyboard_icons = [
    'kbd-capslock-off.png',
    'kbd-numlock-off.png',
    'kbd-scrolllock-off.png',
    'kbd-capslock-on.png',
    'kbd-numlock-on.png',
    'kbd-scrolllock-on.png',
]

install_data(
    keyboard_icons,
    install_dir: join_paths(pkgdatadir, 'icons', 'hicolor', '64x64', 'devices')
)
