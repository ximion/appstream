# Meson definition for AppStream Qt Tests

qt5_test_dep = dependency('qt5', modules: ['Core', 'Test'])

testpaths_h = configuration_data()
testpaths_h.set_quoted('AS_SAMPLE_DATA_PATH', join_paths(source_root, 'tests', 'samples', 'collection'))
configure_file (output: 'testpaths.h', configuration: testpaths_h)

asqt_test_src = [
    'asqt-pool-test.cpp'
]

asqt_test_moc = qt.preprocess (moc_sources: asqt_test_src)

as_test_qt_exe = executable ('as-test_qt',
    [asqt_test_src,
     asqt_test_moc],
    dependencies: [qt5_test_dep],
    include_directories: [include_directories('..')],
    link_with: [appstreamqt_lib],
    cpp_args: asqt_cpp_args
)
test ('as-test_qt',
    as_test_qt_exe,
    env: as_test_env
)
