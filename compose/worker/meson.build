# Meson build configuration for the AppStream Compose media worker

asw_res = glib.compile_resources (
    'asw-resources', 'asc-mediaworker.gresource.xml',
    c_name: 'asw',
    source_dir: ['../../data/compose']
)

asw_src = [
    'asw-canvas.c',
    'asw-font.c',
    'asw-image.c',
    'asw-video.c',
    'asw-worker.c',
]

asw_headers = [
    'asw-canvas.h',
    'asw-font.h',
    'asw-font-private.h',
    'asw-image.h',
    'asw-image-private.h',
    'asw-video.h',
    'asw-worker.h',
]

# dependencies for the mediaworker helper
cairo_dep      = dependency('cairo', version: '>= 1.12')
gdkpixbuf_dep  = dependency('gdk-pixbuf-2.0')
freetype_dep   = dependency('freetype2')
pango_dep      = dependency('pango')
fontconfig_dep = dependency('fontconfig')
if get_option ('svg-support')
    rsvg_dep       = dependency('librsvg-2.0', version: '>= 2.48')
else
    rsvg_dep = dependency('', required: false)
    warning('Building without SVG support. Therefore appstream-compose will be unable to render SVG icons.')
endif

# static library with the worker internals, so unit tests
# can exercise the media processing code in-process
asw_core_lib = static_library('asw-core',
    [asw_src,
     asw_headers,
     asw_res],
    dependencies: [
        appstream_compose_dep,
        m_dep,
        gio_dep,
        gio_unix_dep,
        cairo_dep,
        gdkpixbuf_dep,
        rsvg_dep,
        freetype_dep,
        pango_dep,
        fontconfig_dep,
    ],
    include_directories: [root_inc_dir,
                          include_directories('..')],
    c_args: [
        '-DASC_COMPILATION',
        '-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'
    ],
)

asc_mediaworker_exe = executable('asc-mediaworker',
    ['asw-main.c'],
    link_with: [asw_core_lib],
    dependencies: [
        appstream_compose_dep,
        gio_dep,
    ],
    include_directories: [root_inc_dir,
                          include_directories('..')],
    c_args: [
        '-DASC_COMPILATION',
        '-DI_KNOW_THE_APPSTREAM_COMPOSE_API_IS_SUBJECT_TO_CHANGE'
    ],
    install: true,
    install_dir: get_option('libexecdir'),
)
