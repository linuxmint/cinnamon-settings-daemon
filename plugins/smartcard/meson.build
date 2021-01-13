plugin_name = 'smartcard'

smartcard_sources = [
    'csd-smartcard-manager.c',
    'csd-smartcard.c',
    'main.c',
]

smartcard_deps = [
    common_dep,
    csd_dep,
    libnotify,
    nss
]

executable(
    'csd-smartcard',
    smartcard_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: smartcard_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-smartcard')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-smartcard')
endif

configure_file(
    input: 'cinnamon-settings-daemon-smartcard.desktop.in',
    output: 'cinnamon-settings-daemon-smartcard.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
