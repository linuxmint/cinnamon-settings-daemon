plugin_name = 'housekeeping'

housekeeping_common_sources = [
    'csd-disk-space.c',
    'csd-ldsm-dialog.c',
    'csd-disk-space-helper.c',
]

housekeeping_sources = [
    'csd-housekeeping-manager.c',
    'main.c',
    housekeeping_common_sources,
]

housekeeping_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-housekeeping',
    housekeeping_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: housekeeping_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-housekeeping')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-housekeeping')
endif

test_disk_space_sources = [
    'csd-disk-space-test.c',
    housekeeping_common_sources,
]

executable(
    'test-disk-space',
    test_disk_space_sources,
    dependencies: housekeeping_deps,
    install: false,
)

test_empty_trash_sources = [
    'csd-empty-trash-test.c',
    housekeeping_common_sources,
]

executable(
    'test-empty-trash',
    test_empty_trash_sources,
    dependencies: housekeeping_deps,
    install: false,
)

configure_file(
    input: 'cinnamon-settings-daemon-housekeeping.desktop.in',
    output: 'cinnamon-settings-daemon-housekeeping.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
