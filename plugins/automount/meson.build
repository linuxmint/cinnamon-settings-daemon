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
        '-DG_LOG_DOMAIN="csd-@0@"'.format(plugin_name),
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
