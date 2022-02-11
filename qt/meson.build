# Meson definition for AppStream Qt

asqt_api_level = '2'

add_languages('cpp', native: true)
add_languages('cpp', native: false)

qt = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core'])

asqt_src = [
    'bundle.cpp',
    'category.cpp',
    'component.cpp',
    'contentrating.cpp',
    'icon.cpp',
    'image.cpp',
    'launchable.cpp',
    'metadata.cpp',
    'pool.cpp',
    'provided.cpp',
    'relation.cpp',
    'release.cpp',
    'screenshot.cpp',
    'spdx.cpp',
    'suggested.cpp',
    'translation.cpp',
    'utils.cpp',
    'video.cpp'
]

asqt_pub_headers = [
    'appstreamqt_export.h',
    'bundle.h',
    'category.h',
    'component.h',
    'contentrating.h',
    'icon.h',
    'image.h',
    'launchable.h',
    'metadata.h',
    'pool.h',
    'provided.h',
    'relation.h',
    'release.h',
    'screenshot.h',
    'spdx.h',
    'suggested.h',
    'translation.h',
    'utils.h',
    'video.h'
]

asqt_priv_headers = [
]

asqt_moc = qt.preprocess (
    moc_headers: asqt_pub_headers + asqt_priv_headers,
    #moc_sources: asqt_src
)

asqt_cpp_args = []
if get_option('maintainer')
    asqt_cpp_args = ['-Wno-inline']
endif

appstreamqt_lib = library ('AppStreamQt',
    [asqt_src,
     asqt_pub_headers,
     asqt_priv_headers,
     asqt_moc],
    soversion: asqt_api_level,
    version: as_version,
    dependencies: [qt5_dep,
                   appstream_dep,
                   gio_dep],
    cpp_args: asqt_cpp_args,
    install: true
)

configure_file(
    input: 'version.h.in',
    output: 'version.h',
    install_dir: join_paths(get_option('includedir'), 'AppStreamQt'),
    configuration: conf
)

install_headers(asqt_pub_headers, subdir: 'AppStreamQt')

subdir('tests/')

#
# CMake support
#

cmake_data = configuration_data()
cmake_data.set('LIBDIR_FULL', join_paths(get_option('prefix'), get_option('libdir')))
cmake_data.set('PREFIX', get_option('prefix'))
cmake_data.set('VERSION', as_version)

configure_file (input: 'cmake/AppStreamQtConfig.cmake.in',
                output: 'AppStreamQtConfig.cmake',
                configuration: cmake_data
)
configure_file (input: 'cmake/AppStreamQtConfigVersion.cmake.in',
                output: 'AppStreamQtConfigVersion.cmake',
                configuration: cmake_data
)

install_data (
    join_paths(meson.current_build_dir(), 'AppStreamQtConfig.cmake'),
    join_paths(meson.current_build_dir(), 'AppStreamQtConfigVersion.cmake'),
    install_dir: join_paths(get_option('libdir'), 'cmake', 'AppStreamQt')
)
