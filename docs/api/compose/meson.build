# Meson definition for AppStream Compose API Documentation

#
# Build API documentation for libappstream-compose
#

ascdoc_toml = configure_file(
    input: 'appstream-compose.toml.in',
    output: 'appstream-compose.toml',
    configuration: gid_toml_conf
)

custom_target('doc-api-appstream-compose',
    input: [
      ascdoc_toml,
      asc_gir,
    ],
    output: 'compose-api',
    command: [
      gidocgen_exe,
      'generate',
      '--quiet',
      '--add-include-path=@0@'.format(meson.project_build_root() / 'compose'),
      '--add-include-path=@0@'.format(meson.project_build_root() / 'src'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depends: [
      asc_gir,
    ],
    build_by_default: true,
    install: true,
    install_dir: get_option('prefix') / as_doc_html_target_dir,
)

# make Devhelp work
install_symlink(
    'appstream-compose',
    pointing_to: '..' / '..' / '..' / as_doc_html_target_dir / 'compose-api',
    install_dir: gtk_doc_root
)
