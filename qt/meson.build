# Meson definition for AppStream Qt

asqt_api_level = '2'

add_languages('cpp')

qt = import('qt5')
qt5_dep = dependency('qt5', modules: ['Core'])

asqt_src = [
    'category.cpp',
    'component.cpp',
    'pool.cpp',
    'image.cpp',
    'screenshot.cpp',
    'icon.cpp',
    'provided.cpp',
    'release.cpp',
    'bundle.cpp',
    'suggested.cpp',
    'contentrating.cpp',
    'launchable.cpp',
    'translation.cpp',
    'metadata.cpp'
]

asqt_pub_headers = [
    'appstreamqt_export.h',
    'category.h',
    'component.h',
    'pool.h',
    'image.h',
    'screenshot.h',
    'icon.h',
    'provided.h',
    'release.h',
    'bundle.h',
    'suggested.h',
    'contentrating.h',
    'launchable.h',
    'translation.h',
    'metadata.h'
]

asqt_priv_headers = [
]

asqt_moc = qt.preprocess (
    moc_headers: asqt_pub_headers + asqt_priv_headers,
    #moc_sources: asqt_src
)

appstreamqt_lib = library ('AppStreamQt',
    [asqt_src,
     asqt_pub_headers,
     asqt_priv_headers,
     asqt_moc],
    soversion: asqt_api_level,
    version: as_version,
    dependencies: [qt5_dep,
                   glib_dep,
                   gobject_dep,
                   gio_dep],
    link_with: [appstream_lib],
    include_directories: [appstream_lib_inc],
    install: true
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
