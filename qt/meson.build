# Meson definition for AppStream Qt

asqt_api_level = '2'

if get_option('qt6')
    qt_major_version = '6'
else
    qt_major_version = '5'
endif

add_languages('cpp', native: true)
add_languages('cpp', native: false)

qt = import(f'qt@qt_major_version@')
qt_dep = dependency(f'qt@qt_major_version@', modules: ['Core'])

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
    'systeminfo.cpp',
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
    'systeminfo.h',
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

asqt_cpp_args = ['-DQT_NO_KEYWORDS']
if get_option('maintainer')
    asqt_cpp_args += ['-Wno-inline']
endif

appstreamqt_lib = library ('AppStreamQt',
    [asqt_src,
     asqt_pub_headers,
     asqt_priv_headers,
     asqt_moc],
    soversion: asqt_api_level,
    version: as_version,
    dependencies: [qt_dep,
                   appstream_dep,
                   gio_dep],
    cpp_args: asqt_cpp_args,
    override_options : ['cpp_std=gnu++17'],
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
