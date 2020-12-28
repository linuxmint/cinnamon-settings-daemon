plugin_name = 'xrandr'

xrandr_sources = [
    'csd-xrandr-manager.c',
    'main.c',
]

xrandr_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    libnotify,
    upower_glib,
    wacom,
    xfixes,
]

executable(
    'csd-xrandr',
    xrandr_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: xrandr_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-xrandr')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-xrandr')
endif

configure_file(
    input: 'cinnamon-settings-daemon-xrandr.desktop.in',
    output: 'cinnamon-settings-daemon-xrandr.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
