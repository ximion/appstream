# Meson definition for AppStream Qt Tests

foreach qt_major_version : get_option('qt-versions')

qt = import(f'qt@qt_major_version@')
qt_test_dep = dependency(
    f'qt@qt_major_version@',
    modules: ['Core', 'Test'],
    include_type: 'system'
)

testpaths_h = configuration_data()
testpaths_h.set_quoted('AS_SAMPLE_DATA_PATH', join_paths(source_root, 'tests', 'samples', 'catalog'))
configure_file (output: 'testpaths.h', configuration: testpaths_h)

asqt_test_src = [
    'asqt-pool-test.cpp'
]

asqt_test_moc = qt.preprocess (moc_sources: asqt_test_src)

if qt_major_version == '5'
    appstreamqt_lib = appstreamqt5_lib
else
    appstreamqt_lib = appstreamqt6_lib
endif

as_test_qt_exe = executable(f'as-test_qt@qt_major_version@',
    [asqt_test_src,
     asqt_test_moc],
    dependencies: [qt_test_dep],
    include_directories: [include_directories('..')],
    link_with: [appstreamqt_lib],
    cpp_args: asqt_cpp_args,
)
test(f'as-test_qt@qt_major_version@',
    as_test_qt_exe,
    env: as_test_env
)

endforeach
