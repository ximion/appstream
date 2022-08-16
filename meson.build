project('AppStream', 'c',
  meson_version: '>=0.62',
  default_options: ['c_std=c11', 'cpp_std=gnu++14'],

  license: 'LGPL-2.1+',
  version: '0.15.5',
)

cc = meson.get_compiler('c')
source_root = meson.project_source_root()

as_version = meson.project_version()
as_api_level = '4'

varr = as_version.split('.')
as_major_version = varr[0]
as_minor_version = varr[1]
as_micro_version = varr[2]

#
# Configure files
#
conf = configuration_data()
conf.set('AS_MAJOR_VERSION_CONF', as_major_version)
conf.set('AS_MINOR_VERSION_CONF', as_minor_version)
conf.set('AS_MICRO_VERSION_CONF', as_micro_version)
conf.set_quoted('PACKAGE_VERSION', as_version)
conf.set_quoted('GETTEXT_PACKAGE', 'appstream')
conf.set_quoted('LOCALEDIR',
                join_paths(get_option('prefix'),
                           get_option('localedir')))
conf.set_quoted('LOCALSTATEDIR',
                join_paths(get_option('prefix'),
                           get_option('localstatedir')))
conf.set_quoted('LIBEXECDIR',
                join_paths(get_option('prefix'),
                           get_option('libexecdir')))
conf.set_quoted('DATADIR',
                join_paths(get_option('prefix'),
                           get_option('datadir')))
conf.set_quoted('LIBDIR',
                join_paths(get_option('prefix'),
                           get_option('libdir')))
conf.set_quoted('BINDIR',
                join_paths(get_option('prefix'),
                           get_option('bindir')))
conf.set_quoted('SYSCONFDIR',
                join_paths(get_option('prefix'),
                           get_option('sysconfdir')))
conf.set('HAVE_APT_SUPPORT', get_option('apt-support'))
conf.set('HAVE_STEMMING', get_option('stemming'))

configure_file(output: 'config.h', configuration: conf)
root_inc_dir = include_directories ('.')

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
                         '-Winline',
                         '-Wno-error=comment',
    ]
    add_project_arguments(maintainer_c_args, language: 'c')
    add_project_arguments(maintainer_c_args, language: 'cpp')
endif

if get_option('static-analysis') and host_machine.system() != 'windows'
  if cc.get_id() != 'gcc'
    error('You need to compile with GCC to run the static analyzer!')
  endif

  # enable statuc analyzer
  add_project_arguments(['-fanalyzer'], language : 'c')

  # silence false-positives for NULL dereference.
  # GCC does not appear to recognize that functions like as_is_empty()
  # ensure that a pointer can not be NULL, and therefore is emitting it
  # even in cases where it shouldn't.
  add_project_arguments(['-Wno-analyzer-null-dereference'], language : 'c')

  # make false-positive non-fatal with GCC 12 for now
  if cc.version().version_compare('>11<13')
    add_project_arguments(['-Wno-error=analyzer-use-of-uninitialized-value'], language : 'c')
  endif
endif

# a few compiler warning flags we always want enabled
add_project_arguments('-Werror=implicit-function-declaration', '-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'cpp')
add_project_arguments('-DAS_COMPILATION', language: 'c')

# Vendor extensions in system headers
add_project_arguments('-D_POSIX_C_SOURCE=200112L', language: 'c')

#
# Dependencies
#
glib_dep = dependency('glib-2.0', version: '>= 2.62')
gobject_dep = dependency('gobject-2.0', version: '>= 2.62')
gio_dep = dependency('gio-2.0', version: '>= 2.62')
gio_unix_dep = dependency('gio-unix-2.0', version: '>= 2.62')
curl_dep = dependency('libcurl', version: '>= 7.62')
xml2_dep = dependency('libxml-2.0')
yaml_dep = dependency('yaml-0.1')
xmlb_dep = dependency('xmlb', version: '>= 0.3.6',
                      fallback: ['libxmlb', 'libxmlb_dep'], default_options: ['gtkdoc=false'])

if get_option ('gir')
    # ensure we have a version of GIR that isn't broken with Meson
    # (prior versions failed when any non-GObject library was linked)
    dependency('gobject-introspection-1.0', version: '>=1.56')
endif

stemmer_inc_dirs = include_directories(['/usr/include'])
if get_option('stemming')
    stemmer_lib = cc.find_library('stemmer', required: true)
    if not cc.has_header('libstemmer.h')
        if cc.has_header('libstemmer/libstemmer.h')
            stemmer_inc_dirs = include_directories('/usr/include/libstemmer')
        else
            error('Unable to find Snowball header "libstemmer.h". Please ensure libstemmer/Snowball is installed properly in order to continue.')
        endif
    endif
endif

# use gperf for faster string -> enum matching
gperf = find_program('gperf')

#
# Modules
#
fs   = import('fs')
glib = import('gnome')
i18n = import('i18n')
pkgc = import('pkgconfig')

#
# Directories
#
subdir('src/')
if get_option('compose')
    subdir('compose/')
endif
subdir('tools/')
subdir('po/')
subdir('data/')
subdir('contrib/')
subdir('docs/')
subdir('tests/')
if get_option('qt')
    subdir('qt/')
endif
