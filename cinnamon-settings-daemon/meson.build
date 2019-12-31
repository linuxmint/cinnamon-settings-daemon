csd_sources = [
    'cinnamon-settings-profile.c',
    'cinnamon-settings-profile.h',
    'cinnamon-settings-session.c',
    'cinnamon-settings-session.h',
]

csd_deps = [
    config_h,
    glib,
    gio,
]

if using_logind
    csd_deps += logind
endif

csd_lib = library(
    'csd',
    csd_sources,
    include_directories: include_dirs,
    dependencies: csd_deps,
    install: true,
    install_dir: apilibdir,
)

csd_dep = declare_dependency(
    include_directories: include_dirs,
    link_with: csd_lib,
    dependencies: csd_deps,
)
