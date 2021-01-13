plugin_name = 'background'

background_sources = [
    'csd-background-manager.c',
    'main.c',
]

background_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-background',
    background_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: background_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-background')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-background')
endif

configure_file(
    input: 'cinnamon-settings-daemon-background.desktop.in',
    output: 'cinnamon-settings-daemon-background.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
