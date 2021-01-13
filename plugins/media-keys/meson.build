plugin_name = 'media-keys'

media_keys_sources = [
    'csd-media-keys-manager.c',
    'bus-watch-namespace.c',
    'mpris-controller.c',
    'main.c',
]

media_keys_deps = [
    canberra,
    cinnamon_desktop,
    common_dep,
    csd_dep,
    cvc,
    gudev,
    libnotify,
    math,
]

executable(
    'csd-media-keys',
    media_keys_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: media_keys_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-media-keys')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-media-keys')
endif

configure_file(
    input: 'cinnamon-settings-daemon-media-keys.desktop.in',
    output: 'cinnamon-settings-daemon-media-keys.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
