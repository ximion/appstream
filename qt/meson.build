# Meson definition for AppStream Qt

asqt_api_level = '3'

foreach qt_major : get_option('qt-versions')

if qt_major == '5'
    qt_major_version = '5'
    qt_min_version = '5.15'
    asqt_library_name = 'AppStreamQt5'
else
    qt_major_version = '6'
    qt_min_version = '6.2.4'
    asqt_library_name = 'AppStreamQt'
endif

add_languages('cpp', native: true)
add_languages('cpp', native: false)

qt = import(f'qt@qt_major_version@')
qt_dep = dependency(
    f'qt@qt_major_version@',
    modules: ['Core'],
    version: f'>= @qt_min_version@',
    include_type: 'system'
)

asqt_src = [
    'bundle.cpp',
    'category.cpp',
    'component-box.cpp',
    'component.cpp',
    'contentrating.cpp',
    'developer.cpp',
    'icon.cpp',
    'image.cpp',
    'launchable.cpp',
    'metadata.cpp',
    'pool.cpp',
    'provided.cpp',
    'relation.cpp',
    'relation-check-result.cpp',
    'release.cpp',
    'release-list.cpp',
    'screenshot.cpp',
    'spdx.cpp',
    'suggested.cpp',
    'systeminfo.cpp',
    'translation.cpp',
    'utils.cpp',
    'video.cpp',
]

asqt_pub_headers = [
    'appstreamqt_export.h',
    'bundle.h',
    'category.h',
    'component-box.h',
    'component.h',
    'contentrating.h',
    'developer.h',
    'icon.h',
    'image.h',
    'launchable.h',
    'metadata.h',
    'pool.h',
    'provided.h',
    'relation.h',
    'relation-check-result.h',
    'release.h',
    'release-list.h',
    'screenshot.h',
    'spdx.h',
    'suggested.h',
    'systeminfo.h',
    'translation.h',
    'utils.h',
    'video.h',
]

asqt_priv_headers = [
    'chelpers.h',
]

asqt_moc = qt.preprocess (
    moc_headers: asqt_pub_headers + asqt_priv_headers,
    #moc_sources: asqt_src
)

asqt_cpp_args = ['-DQT_NO_KEYWORDS']
if get_option('maintainer')
    asqt_cpp_args += ['-Wno-inline']
endif

appstreamqt_lib = library(f'@asqt_library_name@',
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
    install: true
)

if qt_major == '5'
    appstreamqt5_lib = appstreamqt_lib
else
    appstreamqt6_lib = appstreamqt_lib
endif

configure_file(
    input: 'version.h.in',
    output: 'version.h',
    install_dir: join_paths(get_option('includedir'), f'@asqt_library_name@'),
    configuration: conf
)

install_headers(asqt_pub_headers, subdir: f'@asqt_library_name@')

#
# CMake support
#

cmake_data = configuration_data()
cmake_data.set('LIBDIR_FULL', join_paths(get_option('prefix'), get_option('libdir')))
cmake_data.set('LIBDIR', get_option('libdir'))
cmake_data.set('PREFIX', get_option('prefix'))
cmake_data.set('VERSION', as_version)

configure_file (input: f'cmake/@asqt_library_name@Config.cmake.in',
                output: f'@asqt_library_name@Config.cmake',
                configuration: cmake_data
)
configure_file (input: f'cmake/@asqt_library_name@ConfigVersion.cmake.in',
                output: f'@asqt_library_name@ConfigVersion.cmake',
                configuration: cmake_data
)

install_data (
    join_paths(meson.current_build_dir(), f'@asqt_library_name@Config.cmake'),
    join_paths(meson.current_build_dir(), f'@asqt_library_name@ConfigVersion.cmake'),
    install_dir: join_paths(get_option('libdir'), 'cmake', asqt_library_name)
)

# end of Qt version loop
endforeach

subdir('tests/')
