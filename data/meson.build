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

itstool_exe = find_program('itstool', required: true)
python_exe = find_program('python3', required: true)


# TODO: We can get rid of this once we can depend on Meson > 0.58
known_locale = run_command(python_exe,
                           '-c',
                           'print(open("'
                                + join_paths(meson.source_root(), 'po', 'LINGUAS') +
                            '", "r", encoding="utf-8").read())').stdout().split()

gmo_files = []
foreach locale : known_locale
    gmo_files += join_paths(meson.build_root(), 'po', locale.strip() + '.gmo')
endforeach

metainfo_i18n = custom_target('metainfo-i18n',
    input : [metainfo_with_relinfo, gmo_files],
    output : 'org.freedesktop.appstream.cli.metainfo.xml',
    command : [itstool_exe,
               '-i', join_paths(meson.current_source_dir(), 'its', 'metainfo.its'),
               '-j', '@INPUT0@',
               '-o', '@OUTPUT@',
               gmo_files,
    ],
    install: true,
    install_dir: metainfo_dir
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
