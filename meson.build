project('AppStream', 'c',
  meson_version : '>=0.40',
  license : 'LGPL-2.1+ and GPL-2.0+',
  version : '0.11.4',
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

configure_file(output : 'config.h', configuration : conf)

#
# Custom C flags
#
sanitizer_libs = []
if get_option('maintainer')
    add_global_arguments(['-Werror',
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
                          '-Winline'],
                          language : 'c'
    )

    if get_option('sanitizers')
        sanitizer_libs = ['asan', 'ubsan']
        add_global_arguments(['-fsanitize=address', '-fsanitize=undefined'], language : 'c')

        if ccompiler.get_id() == 'clang'
            add_global_arguments(['-fno-sanitize-recover=undefined,integer'], language : 'c')
        else
            add_global_arguments(['-fno-sanitize-recover'], language : 'c')
        endif
    endif
endif

# a few compiler warning flags we always want enabled
add_global_arguments('-Werror=implicit-function-declaration', '-Wno-unused-parameter', language : 'c')
add_global_arguments('-DAS_COMPILATION', language : 'c')

#
# Dependencies
#
glib_dep = dependency('glib-2.0', version : '>=2.46')
gobject_dep = dependency('gobject-2.0', version : '>=2.46')
gio_unix_dep = dependency('gio-unix-2.0', version : '>=2.46')
xml2_dep = dependency('libxml-2.0')
yaml_dep = dependency('yaml-0.1')

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
subdir('src')
#subdir(tools)
#subdir(data)
#subdir(contrib)
#subdir(po)
#subdir(docs)
#subdir(tests)
if get_option('qt')
    subdir(qt)
endif
