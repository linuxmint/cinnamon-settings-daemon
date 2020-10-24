plugin_name = 'sound'

sound_sources = [
    'csd-sound-manager.c',
    'main.c',
]

sound_deps = [
    canberra,
    common_dep,
    csd_dep,
    gudev,
    libnotify,
    pulse_glib,
    pulse,
]

executable(
    'csd-sound',
    sound_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: sound_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)
if install_binfiles
    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-sound')
endif

configure_file(
    input: 'cinnamon-settings-daemon-sound.desktop.in',
    output: 'cinnamon-settings-daemon-sound.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
