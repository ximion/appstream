# Meson build configuration for the AppStream library

appstream_lib_inc = include_directories('.')

aslib_src = [
    'as-utils.c',
    # internal
    'as-cache.c',
    'as-curl.c',
    'as-desktop-entry.c',
    'as-distro-extras.c',
    'as-file-monitor.c',
    'as-news-convert.c',
    'as-profile.c',
    'as-stemmer.c',
    'as-tag.c',
    'as-xml.c',
    'as-yaml.c',
    # (mostly) public
    'as-agreement.c',
    'as-agreement-section.c',
    'as-artifact.c',
    'as-branding.c',
    'as-bundle.c',
    'as-category.c',
    'as-checksum.c',
    'as-component.c',
    'as-content-rating.c',
    'as-context.c',
    'as-distro-details.c',
    'as-enums.c',
    'as-icon.c',
    'as-image.c',
    'as-issue.c',
    'as-launchable.c',
    'as-metadata.c',
    'as-pool.c',
    'as-provided.c',
    'as-relation.c',
    'as-release.c',
    'as-review.c',
    'as-screenshot.c',
    'as-spdx.c',
    'as-suggested.c',
    'as-translation.c',
    'as-validator.c',
    'as-validator-issue.c',
    'as-vercmp.c',
    'as-version.c',
    'as-video.c'
]

aslib_pub_headers = [
    'appstream.h',
    'as-agreement.h',
    'as-agreement-section.h',
    'as-artifact.h',
    'as-branding.h',
    'as-bundle.h',
    'as-category.h',
    'as-checksum.h',
    'as-component.h',
    'as-content-rating.h',
    'as-context.h',
    'as-distro-details.h',
    'as-enums.h',
    'as-icon.h',
    'as-image.h',
    'as-issue.h',
    'as-launchable.h',
    'as-metadata.h',
    'as-pool.h',
    'as-provided.h',
    'as-relation.h',
    'as-release.h',
    'as-review.h',
    'as-screenshot.h',
    'as-spdx.h',
    'as-suggested.h',
    'as-translation.h',
    'as-utils.h',
    'as-validator.h',
    'as-validator-issue.h',
    'as-vercmp.h',
    'as-video.h'
]

aslib_priv_headers = [
    'as-agreement-private.h',
    'as-agreement-section-private.h',
    'as-bundle-private.h',
    'as-cache.h',
    'as-checksum-private.h',
    'as-component-private.h',
    'as-content-rating-private.h',
    'as-context-private.h',
    'as-curl.h',
    'as-desktop-entry.h',
    'as-distro-details-private.h',
    'as-distro-extras.h',
    'as-file-monitor.h',
    'as-icon-private.h',
    'as-image-private.h',
    'as-issue-private.h',
    'as-launchable-private.h',
    'as-news-convert.h',
    'as-pool-private.h',
    'as-profile.h',
    'as-relation-private.h',
    'as-release-private.h',
    'as-review-private.h',
    'as-screenshot-private.h',
    'as-stemmer.h',
    'as-tag.h',
    'as-utils-private.h',
    'as-validator-issue-tag.h',
    'as-xml.h',
    'as-yaml.h'
]

as_version_h = configure_file(
    input: 'as-version.h.in',
    output: 'as-version.h',
    install_dir: join_paths(get_option('includedir'), 'appstream'),
    configuration: conf
)

aslib_pub_enums = glib.mkenums_simple('as-enum-types',
    sources: aslib_pub_headers,
    install_header: true,
    install_dir: join_paths(get_option('includedir'), 'appstream')
)

aslib_res = glib.compile_resources (
    'as-resources', 'appstream.gresource.xml',
    c_name: 'as',
    source_dir: ['../data']
)

# gperf sources
aslib_gperf_xml = custom_target(
    'gperf as-xml-tag',
    output: 'as-tag-xml-private.h',
    input: 'as-tag-xml.gperf',
    command: [
        gperf,
        '@INPUT@',
        '--output-file',
        '@OUTPUT@'
    ]
)
aslib_gperf_yaml = custom_target(
    'gperf as-yaml-tag',
    output: 'as-tag-yaml-private.h',
    input: 'as-tag-yaml.gperf',
    command: [
        gperf,
        '@INPUT@',
        '--output-file',
        '@OUTPUT@'
    ]
)
aslib_src = aslib_src + [aslib_gperf_xml, aslib_gperf_yaml]

aslib_deps = [glib_dep,
              gobject_dep,
              gio_unix_dep,
              curl_dep,
              xmlb_dep,
              xml2_dep,
              yaml_dep]
if get_option ('stemming')
    aslib_deps += [stemmer_lib]
endif

appstream_lib = library ('appstream',
    [aslib_src,
     aslib_pub_headers,
     aslib_priv_headers,
     aslib_res,
     aslib_pub_enums],
    soversion: as_api_level,
    version: as_version,
    dependencies: [aslib_deps],
    include_directories: [stemmer_inc_dirs,
                          root_inc_dir],
    install: true
)

appstream_dep = declare_dependency(
    link_with: appstream_lib,
    sources: [
        aslib_pub_headers,
        aslib_pub_enums
    ],
    dependencies: [glib_dep,
                   gobject_dep],
    include_directories: [appstream_lib_inc]
)

pkgc.generate(
    appstream_lib,
    version: as_version,
    name: 'appstream',
    description: 'Library to work with AppStream metadata.',
    filebase: 'appstream',
    subdirs: 'appstream',
    requires: ['glib-2.0', 'gio-2.0', 'gobject-2.0']
)

# Meson adds (in our case) bogus Requires.private lines which break
# builds using libappstream. Fix this issue by post-processing the file.
sed_prog = find_program('sed')
pc_fixup = run_command(sed_prog,
                       '-i',
                       '/^Requires.private\|^Libs.private/ d',
                       join_paths(meson.project_build_root(), 'meson-private', 'appstream.pc'),
                       check: false)
if pc_fixup.returncode() != 0
    error('Unable to fix up the .pc file:\n' + pc_fixup.stderr())
endif

install_headers(aslib_pub_headers, subdir: 'appstream')

if get_option ('gir')
    as_gir_typelib = glib.generate_gir(
        appstream_lib,
        sources: [aslib_src,
                  aslib_pub_headers,
                  aslib_pub_enums,
                  as_version_h],
        nsversion: '1.0',
        namespace: 'AppStream',
        symbol_prefix: 'as',
        identifier_prefix: 'As',
        export_packages: 'appstream',
        includes: [
            'GObject-2.0',
            'Gio-2.0'
        ],
        header: 'appstream.h',
        install: true
    )

    as_gir = as_gir_typelib[0]
    as_typelib = as_gir_typelib[1]
endif
