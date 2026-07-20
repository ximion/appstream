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
vips_dep       = dependency('vips', version: '>= 8.14')
freetype_dep   = dependency('freetype2')
pango_dep      = dependency('pango')
fontconfig_dep = dependency('fontconfig')
if get_option ('svg-support')
    rsvg_dep       = dependency('librsvg-2.0', version: '>= 2.48')
else
    rsvg_dep = dependency('', required: false)
    warning('Building without SVG support. Therefore appstream-compose will be unable to render SVG icons.')
endif

# Image format support in libvips is a build-time option of libvips itself and not
# discoverable via pkg-config, so run a check program if we can. The media worker
# verifies this again at runtime, which also covers cross-compiled builds.
if meson.can_run_host_binaries()
    vips_fmt_check_src = '''
        #include <vips/vips.h>
        int main (int argc, char **argv)
        {
            if (VIPS_INIT ("as-vips-check"))
                return 1;
            if (!vips_type_find ("VipsForeignLoad", "pngload_buffer") ||
                !vips_type_find ("VipsForeignSave", "pngsave"))
                return 2;
            if (!vips_type_find ("VipsForeignLoad", "jxlload_buffer") ||
                !vips_type_find ("VipsForeignSave", "jxlsave"))
                return 3;
        #ifdef CHECK_SVG
            if (!vips_type_find ("VipsForeignLoad", "svgload_buffer"))
                return 4;
        #endif
            vips_shutdown ();
            return 0;
        }'''
    vips_fmt_check = cc.run(vips_fmt_check_src,
        dependencies: [vips_dep],
        args: get_option('svg-support') ? ['-DCHECK_SVG'] : [],
        name: 'libvips has PNG, JPEG-XL (and SVG) support')
    if not vips_fmt_check.compiled() or vips_fmt_check.returncode() != 0
        error('The installed libvips is missing required image format support: ' +
              'it must be built with PNG and JPEG-XL (libjxl) load & save support' +
              (get_option('svg-support') ? ' as well as SVG (librsvg) load support.' : '.'))
    endif
else
    warning('Cross-compiling: unable to verify libvips PNG/JPEG-XL format support at build time, ' +
            'it will be checked when the media worker starts.')
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
        vips_dep,
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
