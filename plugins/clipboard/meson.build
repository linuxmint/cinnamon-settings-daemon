plugin_name = 'clipboard'

clipboard_sources = [
    'csd-clipboard-manager.c',
    'list.c',
    'xutils.c',
    'main.c',
]

clipboard_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-clipboard',
    clipboard_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: clipboard_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-clipboard')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-clipboard')
endif

configure_file(
    input: 'cinnamon-settings-daemon-clipboard.desktop.in',
    output: 'cinnamon-settings-daemon-clipboard.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
