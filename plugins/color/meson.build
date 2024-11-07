plugin_name='color'


built_sources = gnome.gdbus_codegen(
  'cinnamon-session-dbus',
  sources: 'org.gnome.SessionManager.xml',
  interface_prefix: 'org.',
)

sources = files(
  'ccm-edid.c',
  'gnome-datetime-source.c',
  'csd-color-calibrate.c',
  'csd-color-manager.c',
  'csd-color-profiles.c',
  'csd-color-state.c',
  'csd-night-light.c',
  'csd-night-light-common.c',
  'main.c'
)

color_deps = [
    canberra,
    cinnamon_desktop,
    common_dep,
    csd_dep,
    colord,
    lcms,
    libnotify,
    math,
]

executable(
  'csd-' + plugin_name,
  sources + built_sources,
  include_directories: [include_dirs, common_inc],
  dependencies: color_deps,
  c_args: [
      '-DPLUGIN_NAME="@0@"'.format(plugin_name),
      '-DG_LOG_DOMAIN="csd-@0@"'.format(plugin_name),
      '-DBINDIR="@0@"'.format(bindir),
  ],
  install: true,
  install_rpath: join_paths(prefix, apilibdir),
  install_dir: libexecdir
)

meson.add_install_script(ln_script, libexecdir, bindir, 'csd-color')
if libexecdir != pkglibdir
    meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-color')
endif

configure_file(
    input: 'cinnamon-settings-daemon-color.desktop.in',
    output: 'cinnamon-settings-daemon-color.desktop',
    configuration: desktop_conf,
    install_dir: autostartdir,
)

sources = files(
  'ccm-edid.c',
  'ccm-self-test.c',
  'gnome-datetime-source.c',
  'csd-night-light.c',
  'csd-night-light-common.c'
)

# test_unit = 'ccm-self-test'

# exe = executable(
#   test_unit,
#   sources,
#   include_directories: include_dirs,
#   dependencies: color_deps,
#   c_args: '-DTESTDATADIR="@0@"'.format(join_paths(meson.current_source_dir(), 'test-data'))
# )

# envs = ['GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(meson.build_root(), 'data'))]
# test(test_unit, exe, env: envs)
