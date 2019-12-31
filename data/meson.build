pkg_api = 'cinnamon-settings-daemon-3.0'
proj_name = meson.project_name()

enums_header = [
    'csd-enums.h',
]

include_enums = include_directories('.')

install_headers(
    enums_header,
    subdir: join_paths(pkg_api, proj_name),
)

mkenums = gnome.mkenums(
    'org.cinnamon.settings-daemon.enums.xml',
    sources: enums_header,
    comments: '<!-- @comment@ -->',
    fhead: '<schemalist>',
    vhead: '  <@type@ id=\'org.cinnamon.settings-daemon.@EnumName@\'>',
    vprod: '    <value nick=\'@valuenick@\' value=\'@valuenum@\'/>',
    vtail: '  </@type@>',
    ftail: '</schemalist>',
    install_header: true,
    install_dir: schemadir,
)

schemas = [
    'org.cinnamon.settings-daemon.peripherals.gschema.xml',
    'org.cinnamon.settings-daemon.peripherals.wacom.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.power.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.color.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.media-keys.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.xsettings.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.housekeeping.gschema.xml',
    'org.cinnamon.settings-daemon.plugins.xrandr.gschema.xml',
]

schema_conf = configuration_data()
schema_conf.set('GETTEXT_PACKAGE', proj_name)

foreach schema : schemas
    configure_file(
        input: schema + '.in.in',
        output: schema,
        configuration: schema_conf,
        install_dir: schemadir,
    )
endforeach

pkg.generate(
    name: proj_name,
    description: 'cinnamon-settings-daemon specific enumarations',
    subdirs: pkg_api,
    requires: 'glib-2.0',
)
