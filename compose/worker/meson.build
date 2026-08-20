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
    'asw-ipc.c',
    'asw-video.c',
    'asw-worker.c',
]

asw_headers = [
    'asw-canvas.h',
    'asw-font.h',
    'asw-font-private.h',
    'asw-image.h',
    'asw-image-private.h',
    'asw-ipc.h',
    'asw-video.h',
    'asw-worker.h',
]

# dependencies for the mediaworker helper
cairo_dep      = dependency('cairo', version: '>= 1.12')
vips_dep       = dependency('vips', version: '>= 8.14')
freetype_dep   = dependency('freetype2')
pango_dep      = dependency('pango')
fontconfig_dep = dependency('fontconfig')

# Image format support in libvips is a build-time option of libvips itself and not
# discoverable at compile-time for us.
# So we run a check program to warn about missing formats.
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
            if (!vips_type_find ("VipsForeignLoad", "svgload_buffer"))
                return 4;
            vips_shutdown ();
            return 0;
        }'''
    vips_fmt_check = cc.run(vips_fmt_check_src,
        dependencies: [vips_dep],
        name: 'libvips has PNG, JPEG-XL and SVG support')
    if not vips_fmt_check.compiled() or vips_fmt_check.returncode() != 0
        warning('The installed libvips is missing required image format support: it must be ' +
                'built with PNG and JPEG-XL (libjxl) load & save as well as SVG (librsvg) load support.')
    endif
endif

# static library with the worker internals, so unit tests
# can exercise the media processing code in-process
asc_media_lib = static_library('asc-media',
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
        freetype_dep,
        pango_dep,
        fontconfig_dep,
    ],
    include_directories: [root_inc_dir,
                          include_directories('..')],
    c_args: [
        '-DASC_COMPILATION',
    ],
)

# where the arch-dependent media worker helper is installed to
if get_option('mediaworker-dir') == 'libexecdir'
    asc_mediaworker_installdir = get_option('libexecdir')
else
    asc_mediaworker_installdir = get_option('libdir') / 'ascompose'
endif

asc_mediaworker_exe = executable('asc-mediaworker',
    ['asw-main.c'],
    link_with: [asc_media_lib],
    dependencies: [
        appstream_compose_dep,
        gio_dep,
    ],
    include_directories: [root_inc_dir,
                          include_directories('..')],
    c_args: ['-DASC_COMPILATION'],
    install: true,
    install_dir: asc_mediaworker_installdir,
)
