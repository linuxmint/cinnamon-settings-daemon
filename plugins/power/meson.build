plugin_name = 'power'

power_proxy = gnome.gdbus_codegen(
    'csd-power-proxy',
    'org.cinnamon.SettingsDaemon.Power.xml',
    namespace: 'Csd',
    annotations: [
        ['org.cinnamon.SettingsDaemon.Power', 'org.gtk.GDBus.C.Name', 'Power']
    ],
)

power_screen_proxy = gnome.gdbus_codegen(
    'csd-power-screen-proxy',
    'org.cinnamon.SettingsDaemon.Power.Screen.xml',
    namespace: 'Csd',
    annotations: [
        ['org.cinnamon.SettingsDaemon.Power.Screen', 'org.gtk.GDBus.C.Name', 'Screen']
    ],
)

power_keyboard_proxy = gnome.gdbus_codegen(
    'csd-power-keyboard-proxy',
    'org.cinnamon.SettingsDaemon.Power.Keyboard.xml',
    namespace: 'Csd',
    annotations: [
        ['org.cinnamon.SettingsDaemon.Power.Keyboard', 'org.gtk.GDBus.C.Name', 'Keyboard']
    ],
)

power_sources = [
    'csd-power-manager.c',
    'gpm-common.c',
    'gpm-phone.c',
    'gpm-idletime.c',
    'main.c',
    power_proxy,
    power_screen_proxy,
    power_keyboard_proxy,
]

backlight_sources = [
    'csd-backlight-helper.c',
]

power_deps = [
    canberra,
    cinnamon_desktop,
    common_dep,
    csd_dep,
    gio_unix,
    libnotify,
    math,
    upower_glib,
    xext,
]

executable(
    'csd-power',
    power_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: power_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install_rpath: join_paths(prefix, apilibdir),
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-power')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-power')
endif

if gudev.found()
    executable(
        'csd-backlight-helper',
        backlight_sources,
        include_directories: [include_dirs, common_inc],
        dependencies: [power_deps, gudev],
        c_args: [
            # '-DPLUGIN_NAME="@0@"'.format(plugin_name),
        ],
        install: true,
        install_dir: libexecdir,
    )

    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-backlight-helper')
    if libexecdir != pkglibdir
        meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-backlight-helper')
    endif
endif

configure_file(
    input: 'cinnamon-settings-daemon-power.desktop.in',
    output: 'cinnamon-settings-daemon-power.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)

configure_file(
    input: 'org.cinnamon.settings-daemon.plugins.power.policy.in',
    output: 'org.cinnamon.settings-daemon.plugins.power.policy',
    configuration: desktop_conf,
    install_dir: polkitdir,
)
