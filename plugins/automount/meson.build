plugin_name = 'automount'

automount_sources = [
    'csd-automount-manager.c',
    'csd-autorun.c',
    'main.c',
]

automount_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-automount',
    automount_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: automount_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install_rpath: join_paths(prefix, apilibdir),
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-automount')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-automount')
endif

test_automount_dialog_sources = [
    'test-automount-dialog.c',
    'csd-autorun.c',
]

executable(
    'test-automount-dialog',
    test_automount_dialog_sources,
    dependencies: automount_deps,
    install: false,
)

configure_file(
    input: 'cinnamon-settings-daemon-automount.desktop.in',
    output: 'cinnamon-settings-daemon-automount.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)

configure_file(
    input: 'csd-automount.desktop.in',
    output: 'csd-automount.desktop',
    configuration: desktop_conf,
    install_dir: desktopdir,
)
