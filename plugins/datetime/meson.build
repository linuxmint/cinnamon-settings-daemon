
datetime_exported_iface = gnome.gdbus_codegen(
  'csd-exported-datetime',
  'csd-datetime-mechanism.xml',
  interface_prefix: 'org.cinnamon.SettingsDaemon.DateTimeMechanism.',
  namespace: 'Csd',
  annotations: ['org.cinnamon.SettingsDaemon.DateTimeMechanism', 'org.gtk.GDBus.C.Name', 'ExportedDateTime']
)

datetime_common_sources = [
    'system-timezone.c',
    'system-timezone.h',
]

datetime_sources = [
    'csd-datetime-mechanism.c',
    'csd-datetime-mechanism.h',
    'csd-datetime-mechanism-fedora.c',
    'csd-datetime-mechanism-fedora.h',
    'csd-datetime-mechanism-debian.c',
    'csd-datetime-mechanism-debian.h',
    'csd-datetime-mechanism-suse.c',
    'csd-datetime-mechanism-suse.h',
    'csd-datetime-mechanism-main.c',
    datetime_common_sources,
    datetime_exported_iface
]

test_timezone_sources = [
    'test-system-timezone.c',
    datetime_common_sources,
]

datetime_deps = [
    common_dep,
    csd_dep,
    libnotify,
    polkit,
    gio,
    gio_unix
]

if polkit.found()
    executable(
        'csd-datetime-mechanism',
        datetime_sources,
        include_directories: [include_dirs, common_inc],
        dependencies: datetime_deps,
        c_args: [
            '-DPLUGIN_NAME="@0@"'.format(plugin_name),
        ],
        install: true,
        install_dir: libexecdir,
    )

    meson.add_install_script(ln_script, libexecdir, bindir, 'csd-datetime-mechanism')
    if libexecdir != pkglibdir
        meson.add_install_script(ln_script, libexecdir, pkglibdir, 'csd-datetime-mechanism')
    endif

    executable(
        'test-datetime',
        test_timezone_sources,
        dependencies: datetime_deps,
        install: false,
    )
endif

datetime_conf = configuration_data()
datetime_conf.set('LIBEXECDIR', join_paths(prefix, libexecdir))
configure_file(
    input: 'org.cinnamon.SettingsDaemon.DateTimeMechanism.service.in',
    output: 'org.cinnamon.SettingsDaemon.DateTimeMechanism.service',
    configuration: datetime_conf,
    install_dir: dbusservicedir,
)

install_data(
    'org.cinnamon.settingsdaemon.datetimemechanism.policy',
    install_dir: polkitdir,
)
install_data(
    'org.cinnamon.SettingsDaemon.DateTimeMechanism.conf',
    install_dir: dbussystemdir,
)
