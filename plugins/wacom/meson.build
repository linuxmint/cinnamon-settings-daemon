plugin_name = 'wacom'

wacom_resources = gnome.compile_resources(
    'csd-wacom-resources',
    'wacom.gresource.xml',
    c_name: 'csd_wacom',
)

wacom_common_sources = [
    'csd-wacom-device.c',
]

list_wacom_sources = [
    'list-wacom.c',
    wacom_common_sources,
]

wacom_sources = [
    'csd-wacom-manager.c',
    'csd-wacom-osd-window.c',
    'main.c',
    wacom_common_sources,
    wacom_resources,
]

wacom_led_helper_sources = [
    'csd-wacom-led-helper.c',
]

wacom_osd_sources = [
    'test-osd-window.c',
    'csd-wacom-osd-window.c',
    wacom_common_sources,
    wacom_resources,
]

wacom_deps = [
    cinnamon_desktop,
    common_dep,
    csd_dep,
    gtk,
    gudev,
    libnotify,
    librsvg,
    math,
    wacom,
    xfixes,
    xorg_wacom,
    xtst,
]

executable(
    'csd-wacom',
    wacom_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: wacom_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-wacom')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-wacom')
endif

if gudev.found()
    executable(
        'csd-wacom-led-helper',
        wacom_led_helper_sources,
        include_directories: [include_dirs, common_inc, include_enums],
        dependencies: wacom_deps,
        install: true,
        install_dir: libexecdir,
    )

    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-wacom-led-helper')
    if libexecdir != pkglibdir
        meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-wacom-led-helper')
    endif
endif

executable(
    'csd-list-wacom',
    list_wacom_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: wacom_deps,
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-list-wacom')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-list-wacom')
endif

executable(
    'csd-wacom-osd',
    wacom_osd_sources,
    include_directories: [include_dirs, common_inc, include_enums],
    dependencies: wacom_deps,
    install: true,
    install_dir: libexecdir,
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-wacom-osd')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-wacom-osd')
endif

configure_file(
    input: 'cinnamon-settings-daemon-wacom.desktop.in',
    output: 'cinnamon-settings-daemon-wacom.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)

configure_file(
    input: 'org.cinnamon.settings-daemon.plugins.wacom.policy.in',
    output: 'org.cinnamon.settings-daemon.plugins.wacom.policy',
    configuration: desktop_conf,
    install_dir: polkitdir,
)
