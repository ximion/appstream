# Meson definition for AppStream Qt Tests

qt = import('qt6')
qt_test_dep = dependency(
    'qt6',
    modules: ['Core', 'Test'],
    include_type: 'system'
)

testpaths_h = configuration_data()
testpaths_h.set_quoted('AS_SAMPLE_DATA_PATH', join_paths(source_root, 'tests', 'samples', 'catalog'))
configure_file (output: 'testpaths.h', configuration: testpaths_h)

# Workaround for https://github.com/mesonbuild/meson/issues/16087 that was introduced
# with Meson >= 1.8.2
cxx = meson.get_compiler('cpp')
asqt_test_link_args = cxx.get_supported_link_arguments([
    '-Wl,-rpath-link,' + meson.project_build_root() / 'src'
])

asqt_test_pool_src = [
    'asqt-pool-test.cpp'
]
asqt_test_pool_src += qt.preprocess (moc_sources: asqt_test_pool_src)

as_test_qt_pool_exe = executable('as-test_qt-pool',
    [asqt_test_pool_src],
    dependencies: [qt_test_dep],
    include_directories: [include_directories('..')],
    link_with: [appstreamqt_lib],
    cpp_args: asqt_cpp_args,
    link_args: asqt_test_link_args,
)
test('as-test_qt-pool',
    as_test_qt_pool_exe,
    env: as_test_env
)


asqt_test_misc_src = [
    'asqt-misc-test.cpp'
]
asqt_test_misc_src += qt.preprocess (moc_sources: asqt_test_misc_src)

as_test_qt_misc_exe = executable('as-test_qt-misc',
    [asqt_test_misc_src],
    dependencies: [qt_test_dep],
    include_directories: [include_directories('..')],
    link_with: [appstreamqt_lib],
    cpp_args: asqt_cpp_args,
    link_args: asqt_test_link_args,
)
test('as-test_qt-misc',
    as_test_qt_misc_exe,
    env: as_test_env
)
