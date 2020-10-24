desktop_conf = configuration_data()
desktop_conf.set('libexecdir', join_paths(prefix, libexecdir))

install_binfiles = get_option('install_binfiles')
ln_script = join_paths(meson.source_root(), 'install-scripts', 'meson_link_binfiles.py')

# subdir('dummy')

subdir('common')
subdir('a11y-keyboard')
subdir('a11y-settings')
subdir('automount')
subdir('background')
subdir('clipboard')
subdir('cursor')
subdir('datetime')
subdir('housekeeping')
subdir('keyboard')
subdir('media-keys')
subdir('mouse')
subdir('orientation')
subdir('power')
subdir('screensaver-proxy')
subdir('sound')
subdir('xrandr')
subdir('xsettings')

if get_option('use_color')
    subdir('color')
endif

if cups.found()
    subdir('print-notifications')
endif

if nss.found()
    subdir('smartcard')
endif

if wacom.found()
    subdir('wacom')
endif
