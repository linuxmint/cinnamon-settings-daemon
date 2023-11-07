plugin_name = 'settings-remap'

settings_remap_sources = [
    'csd-settings-remap-manager.c',
    'main.c',
]

settings_remap_deps = [
    common_dep,
    csd_dep,
]

executable(
    'csd-settings-remap',
    settings_remap_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: settings_remap_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-settings-remap')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-settings-remap')
endif

configure_file(
    input: 'cinnamon-settings-daemon-settings-remap.desktop.in',
    output: 'cinnamon-settings-daemon-settings-remap.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
