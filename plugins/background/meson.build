plugin_name = 'background'

background_sources = [
    'csd-background-manager.c',
    'monitor-background.c',
    'main.c',
]

background_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    libnotify,
]

if gtk_layer_shell_enabled
    background_deps += gtk_layer_shell
endif

executable(
    'csd-background',
    background_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: background_deps,
    c_args: [
        '-DG_LOG_DOMAIN="csd-@0@"'.format(plugin_name),
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
