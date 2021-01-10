plugin_name = 'usb-protection'

usb_protection_sources = files(
  'csd-usb-protection-manager.c',
  'main.c'
)

usb_protection_deps = [
  common_dep,
  csd_dep,
  gsettings_desktop_dep,
  libnotify
]

executable(
    'csd-usb-protection',
    usb_protection_sources,
    include_directories: [include_dirs, common_inc],
    dependencies: usb_protection_deps,
    c_args: [
        '-DPLUGIN_NAME="@0@"'.format(plugin_name),
    ],
    install: true,
    install_dir: libexecdir,
)
if install_binfiles
    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-usb-protection')
endif

configure_file(
    input: 'cinnamon-settings-daemon-usb-protection.desktop.in',
    output: 'cinnamon-settings-daemon-usb-protection.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)
