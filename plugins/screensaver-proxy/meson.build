plugin_name = 'screensaver-proxy'

screensaver_proxy_sources = [
    'csd-screensaver-proxy-manager.c',
    'main.c',
]

screensaver_proxy_deps = [
    common_dep,
    csd_dep,
    libnotify,
]

executable(
    'csd-screensaver-proxy',
    screensaver_proxy_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: screensaver_proxy_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-screensaver-proxy')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-screensaver-proxy')
endif

configure_file(
    input: 'cinnamon-settings-daemon-screensaver-proxy.desktop.in',
    output: 'cinnamon-settings-daemon-screensaver-proxy.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
