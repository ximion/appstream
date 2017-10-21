# Meson definition for AppStream Tests

as_test_env = []
opt_sanitize = get_option('b_sanitize').split(',')
if opt_sanitize.contains('address') or opt_sanitize.contains('undefined')
    as_test_env = ['ASAN_OPTIONS=detect_leaks=0'] # otherwise we fail on leaks in 3rd-party libraries
endif

as_test_args = [meson.current_source_dir()]

as_test_common_src = [
    'as-test-utils.h',
    'as-test-utils.c'
]

#
# Tests
#

# Basics and utility functions
as_test_basic_exe = executable ('as-test_basic',
    ['test-basics.c',
     as_test_common_src],
    include_directories: [appstream_lib_inc,
                          include_directories('..')],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep,
                   xml2_dep],
    link_with: [appstream_lib],
)
test ('as-test_basic',
    as_test_basic_exe,
    args: as_test_args,
    env: as_test_env
)

# XML read/write
as_test_xml_exe = executable ('as-test_xml',
    ['test-xmldata.c',
     as_test_common_src],
    include_directories: [appstream_lib_inc,
                          include_directories('..')],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep,
                   xml2_dep],
    link_with: [appstream_lib],
)
test ('as-test_xml',
    as_test_xml_exe,
    args: as_test_args,
    env: as_test_env
)

# YAML read/write
as_test_yaml_exe = executable ('as-test_yaml',
    ['test-yamldata.c',
     as_test_common_src],
    include_directories: [appstream_lib_inc,
                          include_directories('..')],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep,
                   xml2_dep],
    link_with: [appstream_lib],
)
test ('as-test_yaml',
    as_test_yaml_exe,
    args: as_test_args,
    env: as_test_env
)

# Metadata Pool
as_test_pool_exe = executable ('as-test_pool',
    ['test-pool.c',
     as_test_common_src],
    include_directories: [appstream_lib_inc,
                          include_directories('..')],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep,
                   xml2_dep],
    link_with: [appstream_lib],
)
test ('as-test_pool',
    as_test_pool_exe,
    args: as_test_args,
    env: as_test_env
)

# Validator
as_test_validate_exe = executable ('as-test_validate',
    ['test-validate.c',
     as_test_common_src],
    include_directories: [appstream_lib_inc,
                          include_directories('..')],
    dependencies: [glib_dep,
                   gobject_dep,
                   gio_dep,
                   xml2_dep],
    link_with: [appstream_lib],
)
test ('as-test_validate',
    as_test_validate_exe,
    args: as_test_args,
    env: as_test_env
)
