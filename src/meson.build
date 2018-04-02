# Meson build configuration for the AppStream library

appstream_lib_inc = include_directories ('.')

aslib_src = [
    'as-utils.c',
        # internal
    'as-context.c',
    'as-xml.c',
    'as-yaml.c',
    'as-variant-cache.c',
    'as-desktop-entry.c',
    'as-distro-extras.c',
    'as-stemmer.c',
        # (mostly) public
    'as-spdx.c',
    'as-metadata.c',
    'as-component.c',
    'as-enums.c',
    'as-provided.c',
    'as-bundle.c',
    'as-pool.c',
    'as-category.c',
    'as-distro-details.c',
    'as-screenshot.c',
    'as-image.c',
    'as-release.c',
    'as-checksum.c',
    'as-validator.c',
    'as-validator-issue.c',
    'as-icon.c',
    'as-translation.c',
    'as-suggested.c',
    'as-content-rating.c',
    'as-launchable.c',
    'as-relation.c'
]

aslib_pub_headers = [
    'appstream.h',
    'as-utils.h',
    'as-spdx.h',
    'as-metadata.h',
    'as-component.h',
    'as-pool.h',
    'as-enums.h',
    'as-provided.h',
    'as-bundle.h',
    'as-category.h',
    'as-distro-details.h',
    'as-screenshot.h',
    'as-image.h',
    'as-release.h',
    'as-checksum.h',
    'as-validator.h',
    'as-validator-issue.h',
    'as-icon.h',
    'as-translation.h',
    'as-suggested.h',
    'as-content-rating.h',
    'as-launchable.h',
    'as-relation.h'
]

aslib_priv_headers = [
    'as-utils-private.h',
    'as-context.h',
    'as-xml.h',
    'as-yaml.h',
    'as-variant-cache.h',
    'as-desktop-entry.h',
    'as-pool-private.h',
    'as-image-private.h',
    'as-component-private.h',
    'as-screenshot-private.h',
    'as-release-private.h',
    'as-distro-extras.h',
    'as-stemmer.h',
    'as-content-rating-private.h',
    'as-bundle-private.h',
    'as-checksum-private.h',
    'as-icon-private.h',
    'as-launchable-private.h',
    'as-relation-private.h'
]

aslib_res = glib.compile_resources (
    'as-resources', 'appstream.gresource.xml',
    c_name : 'as',
    source_dir : ['../data']
)

aslib_deps = [glib_dep, xml2_dep, yaml_dep, gobject_dep, gio_unix_dep]
if get_option ('stemming')
    aslib_deps += [stemmer_lib]
endif

appstream_lib = library ('appstream',
    [aslib_src,
     aslib_pub_headers,
     aslib_priv_headers,
     aslib_res],
    soversion: as_api_level,
    version: as_version,
    dependencies: [aslib_deps],
    include_directories: [stemmer_inc_dirs,
                          include_directories ('..')],
    install: true
)

pkgc.generate (
    version: as_version,
    libraries: appstream_lib,
    name: 'appstream',
    description: 'Library to work with AppStream metadata.',
    filebase: 'appstream',
    subdirs: 'appstream',
    requires: ['glib-2.0', 'gio-2.0']
)

install_headers(aslib_pub_headers, subdir: 'appstream')

if get_option ('gir')
    as_gir_typelib = glib.generate_gir (
        appstream_lib,
        sources: aslib_src + aslib_pub_headers,
        nsversion: '1.0',
        namespace: 'AppStream',
        symbol_prefix: 'as',
        identifier_prefix: 'As',
        export_packages: 'appstream',
        includes: [
            'GObject-2.0',
            'Gio-2.0'
        ],
        extra_args: ['--c-include=appstream.h'],
        install: true
    )

    as_gir = as_gir_typelib[0]
    as_typelib = as_gir_typelib[1]
endif
