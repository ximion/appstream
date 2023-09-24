# Meson definition for AppStream API Documentation

as_doc_html_target_dir = join_paths(as_doc_target_dir, 'html')

gidocgen_dep = dependency('gi-docgen',
  version: '>= 2021.1',
  native: true,
  required: true,
)
gidocgen_exe = find_program('gi-docgen')

gid_toml_conf = configuration_data()
gid_toml_conf.set('version', as_version)

asdoc_toml = configure_file(
    input: 'appstream.toml.in',
    output: 'appstream.toml',
    configuration: gid_toml_conf
)

custom_target('doc-api-appstream',
    input: [
      asdoc_toml,
      as_gir,
    ],
    output: 'api',
    command: [
      gidocgen_exe,
      'generate',
      '--quiet',
      '--add-include-path=@0@'.format(meson.project_build_root() / 'src'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depends: [
      as_gir,
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('prefix') / as_doc_html_target_dir,
)

# create symlink, so tools using gtk-doc can find the API documentation, without us
# duplicating the data in two places.
gtk_doc_root = join_paths(get_option('prefix'), get_option('datadir'), 'gtk-doc', 'html')
install_symlink(
    'appstream',
    pointing_to: '..' / '..' / '..' / as_doc_html_target_dir / 'api',
    install_dir: gtk_doc_root
)

#
# Build API documentation for libappstream-compose,
# if the library is enabled.
#
if get_option('compose')
    subdir('compose/')
endif
