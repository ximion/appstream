# Meson definition for appstreamcli

ascli_src = [
    'appstream-cli.c',
    'ascli-utils.c',
    'ascli-actions-pkgmgr.c',
    'ascli-actions-validate.c',
    'ascli-actions-mdata.c',
    'ascli-actions-misc.c'
]

ascli_exe = executable('appstreamcli',
    [ascli_src],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep],
    link_with: [appstream_lib],
    include_directories: [appstream_lib_inc,
                          include_directories ('..')],
    install: true
)
