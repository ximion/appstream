# Meson definition for AppStream Documentation

#
# Manual pages
#

# where we install all our documentation to
as_doc_target_dir = join_paths(get_option('datadir'), 'doc', 'appstream')

# make manual pages
manpages_xsl = 'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl'
xsltproc = find_program('xsltproc')
if run_command([xsltproc, '--nonet', manpages_xsl],
                check: false).returncode() != 0
    error('Unable to find Docbook XSL stylesheets for man pages')
endif

custom_target('man-appstreamcli',
    input: 'xml/man/appstreamcli.1.xml',
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
        manpages_xsl,
        '@INPUT@'
    ]
)

if get_option('compose')
    custom_target('man-appstream-compose',
        input: 'xml/man/appstreamcli-compose.1.xml',
        output: 'appstreamcli-compose.1',
        install: true,
        install_dir: join_paths(get_option('mandir'), 'man1'),
        command: [
            xsltproc,
            '--nonet',
            '--stringparam', 'man.output.quietly', '1',
            '--stringparam', 'funcsynopsis.style', 'ansi',
            '--stringparam', 'man.th.extra1.suppress', '1',
            '-o', '@OUTPUT@',
            manpages_xsl,
            '@INPUT@'
        ]
    )
endif

if get_option('docs') or get_option('apidocs')
  if get_option('gir')
    subdir('api/')
  endif
endif

#
# Documentation
#

gen_doc_valsec_src = [
    'gen-valtag-sections.c',
    '../src/as-validator-issue-tag.h',
    '../compose/asc-hint-tags.h',
    '../compose/asc-hint-tags.c',
]

gen_doc_valsec_exe = executable('gen-validator-tag-sections',
    [gen_doc_valsec_src],
    dependencies: [appstream_dep,
                   gio_dep],
    include_directories: [root_inc_dir,
                          include_directories ('../compose')],
    c_args: ['-DASC_COMPILATION', '-Wno-inline'],
    install: false,
)

as_doc_src = [
    'xml/APIDoc.xml',
    'xml/AppStream.xml',
    'xml/Author_Group.xml',
    'xml/Book_Info.xml',
    'xml/CatalogData.xml',
    'xml/catalog-iconcache.xml',
    'xml/catalog-xmldata.xml',
    'xml/catalog-yamldata.xml',
    'xml/Legal_Notice.xml',
    'xml/man/appstreamcli.1.xml',
    'xml/Manpages.xml',
    'xml/MetaInfo.xml',
    'xml/metainfo-addon.xml',
    'xml/metainfo-codec.xml',
    'xml/metainfo-component.xml',
    'xml/metainfo-consoleapp.xml',
    'xml/metainfo-desktopapp.xml',
    'xml/metainfo-driver.xml',
    'xml/metainfo-firmware.xml',
    'xml/metainfo-font.xml',
    'xml/metainfo-icontheme.xml',
    'xml/metainfo-inputmethod.xml',
    'xml/metainfo-localization.xml',
    'xml/metainfo-operatingsystem.xml',
    'xml/metainfo-repository.xml',
    'xml/metainfo-runtime.xml',
    'xml/metainfo-service.xml',
    'xml/metainfo-webapp.xml',
    'xml/releases-data.xml',
    'xml/Miscellaneous.xml',
    'xml/misc-vercmp.xml',
    'xml/misc-urihandler.xml',
    'xml/Project_Desc.xml',
    'xml/Quickstart.xml',
    'xml/quickstart-addons.xml',
    'xml/quickstart-desktopapps.xml',
    'xml/quickstart-packaging.xml',
    'xml/quickstart-translation.xml',
    'xml/Validation.xml',
]

hljs_installed_file = '/usr/share/javascript/highlight.js/highlight.min.js'
if get_option('docs')
    daps_exe = find_program('daps')
    python_exe = find_program('python3', 'python')

    build_docs_cmd = [
            python_exe,
            join_paths(meson.current_source_dir(), 'doc-build-helper.py'),
            '--build',
            '--src', meson.current_source_dir(),
            '--builddir', meson.current_build_dir(),
            '--valsec-gen', gen_doc_valsec_exe.path(),
            'AppStream'
        ]

    make_docs_target = custom_target('make-docs',
        input: ['DC-AppStream',
                as_doc_src],
        output: ['docs_built.stamp'],
        depends: [gen_doc_valsec_exe],
        build_by_default: true,
        command: build_docs_cmd
    )

    # helper if you only and always want to rebuild the docs
    run_target('documentation',
                command: build_docs_cmd,
                depends: [gen_doc_valsec_exe])

    if get_option('install-docs')
        install_subdir('html', install_dir: as_doc_target_dir)
        if fs.is_file(hljs_installed_file)
            install_symlink(
                'highlight.min.js',
                pointing_to: hljs_installed_file,
                install_dir: join_paths(get_option('prefix'), as_doc_target_dir, 'html', 'static', 'js')
            )
        endif
    endif

    # add an extra testcase for documentation validation
    test('as-validate_docs',
          python_exe,
          args: [join_paths(meson.current_source_dir(), 'doc-build-helper.py'),
                 '--validate',
                 '--src', meson.current_source_dir(),
                 '--valsec-gen', gen_doc_valsec_exe.path()],
          timeout: 60
    )
elif get_option('install-docs')
    if fs.is_dir(join_paths(meson.current_source_dir(), 'html'))
        # install documentation, if it exists
        install_subdir('html', install_dir: as_doc_target_dir)
        if fs.is_file(hljs_installed_file)
            install_symlink(
                'highlight.min.js',
                pointing_to: hljs_installed_file,
                install_dir: join_paths(get_option('prefix'), as_doc_target_dir, 'html', 'static', 'js')
            )
        endif
    endif
endif
