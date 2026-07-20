# Meson build configuration for the AppStream Compose library

appstream_compose_lib_inc = include_directories ('.')

ascompose_src = [
    'asc-compose.c',
    'asc-directory-unit.c',
    'asc-globals.c',
    'asc-hint.c',
    'asc-hint-tags.c',
    'asc-icon-policy.c',
    'asc-media.c',
    'asc-media-ipc.c',
    'asc-result.c',
    'asc-unit.c',
    'asc-utils-l10n.c',
    'asc-utils-metainfo.c',
    'asc-utils-screenshots.c',
    'asc-utils-fonts.c',
    'asc-utils.c'
]

ascompose_pub_headers = [
    'appstream-compose.h',
    'asc-compose.h',
    'asc-directory-unit.h',
    'asc-globals.h',
    'asc-hint.h',
    'asc-icon-policy.h',
    'asc-media.h',
    'asc-result.h',
    'asc-unit.h',
    'asc-utils.h'
]

ascompose_pub_enums = glib.mkenums_simple('asc-enums-types',
    sources: ascompose_pub_headers,
    install_header: true,
    install_dir: join_paths(get_option('includedir'), 'appstream-compose')
)

ascompose_priv_headers = [
    'asc-globals-private.h',
    'asc-hint-tags.h',
    'asc-media-ipc.h',
    'asc-media-private.h',
    'asc-utils-l10n.h',
    'asc-utils-metainfo.h',
    'asc-utils-screenshots.h',
    'asc-utils-fonts.h',
]

# extra dependencies we need just for AppStream Compose
m_dep        = cc.find_library('m', required: false)
blake3_dep   = dependency('libblake3', required: get_option('blake3-support'))
gio_unix_dep = dependency('gio-unix-2.0', version: '>= 2.62')

appstream_compose_lib = library ('appstream-compose',
    [ascompose_src,
     ascompose_pub_headers,
     ascompose_priv_headers,
     ascompose_pub_enums],
    soversion: '0',
    version: as_version,
    dependencies: [
        appstream_dep,
        m_dep,
        blake3_dep,
        fyaml_dep,
        gio_dep,
        gio_unix_dep,
    ],
    include_directories: [root_inc_dir],
    c_args: ['-DASC_COMPILATION',
             '-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'],
    install: true
)

pkgc.generate(
    appstream_compose_lib,
    version: as_version,
    name: 'appstream-compose',
    description: 'Helper functions to generate AppStream metadata and auxiliary data.',
    filebase: 'appstream-compose',
    subdirs: 'appstream-compose',
    requires: [
        'glib-2.0',
        'gio-2.0',
        'gobject-2.0',
        'appstream'
    ],
)

appstream_compose_dep = declare_dependency(
    link_with: appstream_compose_lib,
    sources: [
        ascompose_pub_headers,
        ascompose_pub_enums
    ],
    dependencies: [
        appstream_dep,
        glib_dep,
        gobject_dep,
        gio_dep
    ],
    include_directories: [appstream_compose_lib_inc]
)

# Meson adds (in our case) bogus Requires.private lines which break
# builds using libappstream. Fix this issue by post-processing the file.
sed_prog = find_program('sed')
pc_fixup = run_command(sed_prog,
                       '-i.bak',
                       '/^Requires.private\|^Libs.private/ d',
                       join_paths(meson.project_build_root(), 'meson-private', 'appstream-compose.pc'),
                       check: false)
if pc_fixup.returncode() != 0
    error('Unable to fix up the .pc file:\n' + pc_fixup.stderr())
endif

install_headers(ascompose_pub_headers, subdir: 'appstream-compose')

if get_option ('gir')
    asc_gir_typelib = glib.generate_gir(
        appstream_compose_lib,
        sources: ascompose_src + ascompose_pub_headers + ascompose_pub_enums,
        nsversion: '1.0',
        namespace: 'AppStreamCompose',
        symbol_prefix: 'asc',
        identifier_prefix: 'Asc',
        export_packages: 'appstream-compose',
        includes: [
            as_gir,
            'GObject-2.0',
            'Gio-2.0',
        ],
        header: 'appstream-compose.h',
        include_directories: [appstream_lib_inc],
        dependencies: [appstream_dep],
        extra_args: [
            '-DASC_COMPILATION',
            '-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'
        ],
        install: true
    )

    asc_gir = asc_gir_typelib[0]
    asc_typelib = asc_gir_typelib[1]
endif
