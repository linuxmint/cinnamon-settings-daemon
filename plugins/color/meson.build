plugin_name = 'color'

color_sources = [
    'csd-color-manager.c',
    'gcm-profile-store.c',
    'gcm-dmi.c',
    'gcm-edid.c',
    'main.c',
]

color_deps = [
    canberra,
    cinnamon_desktop,
    common_dep,
    csd_dep,
    colord,
    lcms,
    libnotify,
    math,
]

executable(
    'csd-color',
    color_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: color_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
        '-DBINDIR="@0@"'.format(bindir),
    ],
    install_rpath: join_paths(prefix, apilibdir),
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-color')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-color')
endif

test_color_sources = [
    'gcm-dmi.c',
    'gcm-edid.c',
    'gcm-self-test.c',
]

executable(
    'test-color',
    test_color_sources,
    dependencies: color_deps,
    c_args: [
        '-DTESTDATADIR="@0@"'.format(join_paths(meson.current_source_dir(), 'test-data')),
    ],
    install: false,
)

configure_file(
    input: 'cinnamon-settings-daemon-color.desktop.in',
    output: 'cinnamon-settings-daemon-color.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
