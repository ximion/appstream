# Meson definition for AppStream data

install_data ('its/metainfo.its', install_dir: 'share/gettext/its')
install_data ('its/metainfo.loc', install_dir: 'share/gettext/its')

i18n.merge_file (
    input:  'org.freedesktop.appstream.cli.metainfo.xml',
    output: 'org.freedesktop.appstream.cli.metainfo.xml',
    type: 'xml',
    data_dirs: [meson.current_source_dir()],
    po_dir: join_paths (source_root, 'po'),
    install: true,
    install_dir: join_paths (get_option ('datadir'), 'metainfo')
)

install_data ('appstream.conf', install_dir: '/etc')
