# Meson definition for AppStream Documentation


#
# Manual pages
#

# where we install all our documentation to
as_doc_target_dir = join_paths(get_option('datadir'), 'doc', 'appstream')

# make manual pages
xsltproc = find_program('xsltproc')
custom_target('man-appstreamcli',
    input: 'sources/man/appstreamcli.1.xml',
    output: 'appstreamcli.1',
    install: true,
    install_dir: join_paths(get_option('mandir'), 'man1'),
    command: [
        xsltproc,
        '--nonet',
        '--stringparam', 'man.output.quietly', '1',
        '--stringparam', 'funcsynopsis.style', 'ansi',
        '--stringparam', 'man.th.extra1.suppress', '1',
        '-o', '@OUTPUT@',
        'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
        '@INPUT@'
    ]
)

if get_option('docs') or get_option('apidocs')
    subdir('api/')
endif

#
# Documentation
#
as_doc_version = '0.12'

as_doc_src = [
    'sources/APIDoc.xml',
    'sources/AppStream.xml',
    'sources/Author_Group.xml',
    'sources/Book_Info.xml',
    'sources/collection/CollectionData.xml',
    'sources/collection/iconcache.xml',
    'sources/collection/xmldata.xml',
    'sources/collection/yamldata.xml',
    'sources/Legal_Notice.xml',
    'sources/man/appstreamcli.1.xml',
    'sources/Manpages.xml',
    'sources/metainfo/addon.xml',
    'sources/metainfo/codec.xml',
    'sources/metainfo/component.xml',
    'sources/metainfo/consoleapp.xml',
    'sources/metainfo/desktopapp.xml',
    'sources/metainfo/driver.xml',
    'sources/metainfo/firmware.xml',
    'sources/metainfo/font.xml',
    'sources/metainfo/inputmethod.xml',
    'sources/metainfo/localization.xml',
    'sources/metainfo/MetaInfo.xml',
    'sources/metainfo/service.xml',
    'sources/metainfo/webapp.xml',
    'sources/Project_Desc.xml',
    'sources/quickstart/addons.xml',
    'sources/quickstart/desktop-apps.xml',
    'sources/quickstart/packaging.xml',
    'sources/quickstart/Quickstart.xml',
    'sources/quickstart/translation.xml',
    'sources/Revision_History.xml',
    'sources/services/screenshots.xml',
    'sources/services/Services.xml',
    'sources/services/urlhandler.xml'
]

if get_option('docs')
    publican_exe = find_program('publican')
    python_exe = find_program('python3')

    build_docs_cmd = [
            python_exe,
            join_paths(meson.current_source_dir(), 'doc-build-helper.py'),
            '--src=' + meson.current_source_dir(),
            '--ws='  + meson.current_build_dir(),
            '--out=' + meson.current_source_dir(),
            'AppStream',
            as_doc_version
        ]

    make_docs_target = custom_target('make-docs',
        input: ['publican.cfg',
                as_doc_src],
        output: ['docs_built.stamp'],
        build_by_default: true,
        command: build_docs_cmd
    )

    # helper if you only and always want to rebuild the docs
    run_target('documentation', command: build_docs_cmd)

    install_subdir('html', install_dir: as_doc_target_dir)
else
    if run_command('[', '-d', join_paths(meson.current_source_dir(), 'html'), ']').returncode() == 0
        # install documentation, if it exists
        install_subdir('html', install_dir: as_doc_target_dir)
    endif
endif
