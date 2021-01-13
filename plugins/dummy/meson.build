plugin_name = 'dummy'

dummy_sources = [
    'csd-dummy-manager.h',
    'csd-dummy-manager.c',
    'main.c',
]

dummy_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-dummy',
    dummy_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: dummy_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-dummy')
