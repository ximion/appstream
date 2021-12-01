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

if meson.version().version_compare('>=0.60')
    # starting with Meson 0.60 we can use a crazy hack to generate sane output data
    # using itstool - hopefully Meson will gain the ability to do this natively soon
    itstool_exe = find_program('itstool', required: true)
    python_exe = find_program('python3', required: true)

    known_locale = run_command(python_exe,
                           '-c',
                           'print(open("'
                                + join_paths(meson.source_root(), 'po', 'LINGUAS') +
                            '", "r", encoding="utf-8").read())').stdout().split()

    metainfo_i18n = custom_target('metainfo-i18n',
        input : [metainfo_with_relinfo, i18n_result[0]],
        output : 'org.freedesktop.appstream.cli.metainfo.xml',
        command : [python_exe,
                join_paths(meson.current_source_dir(), 'translate-metainfo.py'),
                itstool_exe.path(),
                '@INPUT0@',
                '@OUTPUT@',
                'appstream',
                join_paths(meson.current_source_dir(), 'its', 'metainfo.its'),
                join_paths(meson.build_root(), 'po'),
                known_locale,
        ],
        install: true,
        install_dir: metainfo_dir
    )
else
    # generates XML with mangled description markup tags, but better than nothing...
    metainfo_i18n = i18n.merge_file (
        input:  metainfo_with_relinfo,
        output: 'org.freedesktop.appstream.cli.metainfo.xml',
        type: 'xml',
        data_dirs: [meson.current_source_dir()],
        po_dir: join_paths (source_root, 'po'),
        install: true,
        install_dir: metainfo_dir
    )
endif

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
