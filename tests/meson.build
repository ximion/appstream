# Meson definition for AppStream Tests

subdir('installed-tests/')

as_test_env = []
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
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
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
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
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
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
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
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
    include_directories: [root_inc_dir]
)
test ('as-test_pool',
    as_test_pool_exe,
    args: as_test_args,
    env: as_test_env,
    timeout: 90
)

# Validator
as_test_validate_exe = executable ('as-test_validate',
    ['test-validate.c',
     as_test_common_src],
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
)
test ('as-test_validate',
    as_test_validate_exe,
    args: as_test_args,
    env: as_test_env
)

# Performance
as_test_perf_exe = executable ('as-test_perf',
    ['test-performance.c',
     as_test_common_src],
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
)
test ('as-test_perf',
    as_test_perf_exe,
    args: as_test_args,
    env: as_test_env,
    timeout: 90
)

# Miscelaneous utilities
as_test_misc_exe = executable ('as-test_misc',
    ['test-misc.c',
     as_test_common_src],
    dependencies: [appstream_dep,
                   gio_dep,
                   xml2_dep],
)
test ('as-test_misc',
    as_test_misc_exe,
    args: as_test_args,
    env: as_test_env
)

# AppStream Compose tests
if get_option('compose')
    as_test_compose_exe = executable ('as-test_compose',
        ['test-compose.c',
         as_test_common_src],
        dependencies: [appstream_compose_dep,
                       gio_dep,
                       freetype_dep,
                       gdkpixbuf_dep],
        c_args: ['-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'],
    )
    test ('as-test_compose',
        as_test_compose_exe,
        args: as_test_args,
        env: as_test_env +
            ['LSAN_OPTIONS=suppressions=' +
                join_paths(meson.current_source_dir(), 'lsan-suppr.txt')]
    )
endif
