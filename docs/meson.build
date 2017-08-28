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

subdir('api/')

#
# Documentation
#
as_doc_version = '0.11'

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

if get_option('documentation')
    publican_exe = find_program('publican')

    # we need to assemble our documentation build-directory before we can built it
    doc_assemble_target = custom_target('doc-assemble',
        input: ['publican.cfg',
                as_doc_src],
        output: 'publican_ws_invalid',
        command: [
            join_paths(meson.current_source_dir(), 'prepare-docbuild.sh'),
            'source_dir=' + meson.current_source_dir(),
            'build_dir='  + join_paths(meson.current_build_dir(), 'publican_ws')
        ]
    )

    publican_target = custom_target('publican-publish',
        output: 'doc_pub_invalid',
        input: [as_doc_src],
        depends: [doc_assemble_target],
        command: [
            publican_exe,
            'build',
            '--src_dir=' + join_paths(meson.current_build_dir(), 'publican_ws'),
            '--pub_dir=../doc_pub',
            '--langs=en-US',
            '--publish',
            '--formats=html'
        ]
    )

    clear_docs_target = custom_target('clear-docs',
        input: [as_doc_src],
        output: 'doc_clear',
        depends: [publican_target],
        command: [
            'rm',
            '-rf',
            join_paths(meson.current_source_dir(), 'html')
        ]
    )

    move_docs_target = custom_target('move-docs',
        input: [as_doc_src],
        output: 'html',
        depends: [publican_target, clear_docs_target],
        command: [
            'mv',
            join_paths(meson.current_build_dir(), 'doc_pub', 'en-US', 'AppStream', as_doc_version, 'html', 'AppStream'),
            join_paths(meson.current_source_dir(), 'html')
        ]
    )

    run_target('documentation', depends: move_docs_target, command: 'true')

    install_subdir('html', install_dir: as_doc_target_dir)
endif
