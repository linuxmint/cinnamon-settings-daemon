include_enums = include_directories('.')

mkenums = gnome.mkenums(
    'org.cinnamon.settings-daemon.enums.xml',
    sources: ['csd-enums.h'],
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
schema_conf.set('GETTEXT_PACKAGE', meson.project_name())

foreach schema : schemas
    configure_file(
        input: schema + '.in.in',
        output: schema,
        configuration: schema_conf,
        install_dir: schemadir,
    )
endforeach

