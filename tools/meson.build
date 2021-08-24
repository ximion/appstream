# Meson definition for appstreamcli

ascli_src = [
    'appstreamcli.c',
    'ascli-utils.c',
    'ascli-actions-pkgmgr.c',
    'ascli-actions-validate.c',
    'ascli-actions-mdata.c',
    'ascli-actions-misc.c',
]

ascli_exe = executable('appstreamcli',
    [ascli_src],
    dependencies: [appstream_dep,
                   gio_dep],
    include_directories: [root_inc_dir],
    install: true
)

if get_option('compose')
    ascompose_src = [
        'appstream-compose.c',
        'ascli-utils.c',
    ]

    ascompose_exe = executable('appstreamcli-compose',
        [ascompose_src],
        dependencies: [appstream_dep,
                       appstream_compose_dep,
                       gio_dep],
        include_directories: [root_inc_dir],
        c_args: ['-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'],
        install_dir: get_option('libexecdir'),
        install: true
    )
endif
