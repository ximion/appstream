# Meson definition for AppStream data

metainfo_dir = join_paths(get_option ('datadir'), 'metainfo')

install_data('its/metainfo.its',
             install_dir: join_paths(get_option('datadir'), 'gettext', 'its'))
install_data('its/metainfo.loc',
              install_dir: join_paths(get_option('datadir'), 'gettext', 'its'))

# NOTE: We do not translate the release notes on purpose here.
# If you do want to give translators a chance to translate them,
# ascli news-to-metainfo needs to produce a temporary file to translate
# prior to running (x)gettext on the file.
metainfo_with_relinfo = custom_target('gen-output',
    input : ['../NEWS', 'org.freedesktop.appstream.cli.metainfo.xml'],
    output : ['nol10n_withrelinfo_org.freedesktop.appstream.cli.metainfo.xml'],
    command : [ascli_exe, 'news-to-metainfo', '--limit=6', '@INPUT0@', '@INPUT1@', '@OUTPUT@']
)

metainfo_i18n = i18n.itstool_join(
    input:  metainfo_with_relinfo,
    output: 'org.freedesktop.appstream.cli.metainfo.xml',
    mo_targets: i18n_result[0],
    its_files: [join_paths(meson.current_source_dir(), 'its', 'metainfo.its')],
    install: true,
    install_dir: metainfo_dir,
)

test('as-validate_metainfo.cli',
    ascli_exe,
    args: ['validate',
           '--pedantic',
           '--no-net',
           metainfo_i18n]
)

install_data('appstream.conf',
             install_dir: get_option('sysconfdir'))

if get_option('compose')
    ascompose_metainfo = 'org.freedesktop.appstream.compose.metainfo.xml'
    test('as-validate_metainfo.compose',
         ascli_exe,
         args: ['validate',
                '--pedantic',
                '--no-net',
                join_paths(meson.current_source_dir(), ascompose_metainfo)]
    )
    install_data(ascompose_metainfo, install_dir: metainfo_dir)
endif
