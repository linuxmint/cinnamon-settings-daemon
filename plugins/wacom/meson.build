plugin_name = 'wacom'

sources = files(
  'csd-wacom-manager.c',
  'csd-wacom-oled.c',
  'main.c'
)

deps = [
  gtk,
  common_dep,
  libnotify,
  librsvg,
  math,
  pango,
  wacom
]

cflags = [
  '-DPLUGIN_NAME="@0@"'.format(plugin_name),
]

executable(
  'csd-' + plugin_name,
  sources,
  include_directories: [include_dirs, include_enums, common_inc],
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_rpath: join_paths(prefix, apilibdir),
  install_dir: libexecdir
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-wacom')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-wacom')
endif

led_deps = [
  gudev,
  math
]

programs = [
  'csd-wacom-led-helper',
  'csd-wacom-oled-helper',
]

foreach program: programs
  executable(
    program,
    program + '.c',
    include_directories: include_dirs,
    dependencies: led_deps,
    install: true,
    install_dir: libexecdir
  )
 endforeach

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
