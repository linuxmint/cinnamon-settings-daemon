plugin_name = 'a11y-keyboard'

a11y_keyboard_sources = [
    'csd-a11y-keyboard-manager.c',
    'csd-a11y-preferences-dialog.c',
    'main.c',
]

a11y_keyboard_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-a11y-keyboard',
    a11y_keyboard_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: a11y_keyboard_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-a11y-keyboard')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-a11y-keyboard')
endif

test_a11y_prefs_dialog_sources = [
    'csd-a11y-preferences-dialog.c',
    'test-a11y-preferences-dialog.c',
]

executable(
    'test-a11y-preferences-dialog',
    test_a11y_prefs_dialog_sources,
    dependencies: a11y_keyboard_deps,
    install: false,
)

configure_file(
    input: 'cinnamon-settings-daemon-a11y-keyboard.desktop.in',
    output: 'cinnamon-settings-daemon-a11y-keyboard.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)

install_data(
    'csd-a11y-preferences-dialog.ui',
    install_dir: pkgdatadir,
)
