project('AppStream', 'c',
  meson_version: '>=0.42',
  default_options: ['c_std=gnu11', 'cpp_std=gnu++14'],

  license: 'LGPL-2.1+ and GPL-2.0+',
  version: '0.12.0',
)

ccompiler = meson.get_compiler('c')
source_root = meson.source_root()

as_version = meson.project_version()
as_api_level = '4'

#
# Configure files
#
conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', as_version)
conf.set_quoted('GETTEXT_PACKAGE', 'appstream')
conf.set_quoted('LOCALEDIR', get_option('localedir'))
conf.set_quoted('LOCALSTATEDIR', get_option('localstatedir'))

if get_option('apt-support')
    conf.set('HAVE_APT_SUPPORT', 1)
endif
if get_option('stemming')
    conf.set('HAVE_STEMMING', 1)
endif

configure_file(output: 'config.h', configuration: conf)

#
# Custom C flags
#
sanitizer_libs = []
if get_option('maintainer')
    maintainer_c_args = ['-Werror',
                         '-Wall',
                         '-Wextra',
                         '-Wcast-align',
                         '-Wno-uninitialized',
                         '-Wempty-body',
                         '-Wformat-security',
                         '-Winit-self',
                         '-Wnull-dereference',
                         '-Wfloat-equal',
                         #'-Wformat-signedness',
                         '-Winline'
    ]
    add_global_arguments(maintainer_c_args, language: 'c')
    add_global_arguments(maintainer_c_args, language: 'cpp')
endif

# a few compiler warning flags we always want enabled
add_global_arguments('-Werror=implicit-function-declaration', '-Wno-unused-parameter', language: 'c')
add_global_arguments('-Werror=implicit-function-declaration', '-Wno-unused-parameter', language: 'cpp')
add_global_arguments('-DAS_COMPILATION', language : 'c')

#
# Dependencies
#
glib_dep = dependency('glib-2.0', version : '>=2.46')
gobject_dep = dependency('gobject-2.0', version : '>=2.46')
gio_dep = dependency('gio-2.0', version : '>=2.46')
gio_unix_dep = dependency('gio-unix-2.0', version : '>=2.46')
xml2_dep = dependency('libxml-2.0')
yaml_dep = dependency('yaml-0.1')

if get_option ('gir')
    # ensure we have a version of GIR that isn't broken with Meson
    # (prior versions failed when any non-GObject library was linked)
    dependency('gobject-introspection-1.0', version: '>=1.54')
endif

stemmer_inc_dirs = include_directories(['/usr/include'])
if get_option('stemming')
    stemmer_lib = ccompiler.find_library('stemmer', required: true)
    if not ccompiler.has_header('libstemmer.h')
        if ccompiler.has_header('libstemmer/libstemmer.h')
            stemmer_inc_dirs = include_directories('/usr/include/libstemmer')
        else
            error('Unable to find Snowball header "libstemmer.h". Please ensure libstemmer/Snowball is installed properly in order to continue.')
        endif
    endif
endif

#
# Modules
#
glib = import('gnome')
i18n = import('i18n')
pkgc = import('pkgconfig')

#
# Directories
#
subdir('src/')
subdir('tools/')
subdir('data/')
subdir('contrib/')
subdir('po/')
subdir('docs/')
subdir('tests/')
if get_option('qt')
    subdir('qt/')
endif
