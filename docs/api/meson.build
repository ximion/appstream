# Meson definition for AppStream API Documentation

as_api_doc_target_dir = join_paths(as_doc_target_dir, 'html', 'api')

glib.gtkdoc (
    'appstream',
    src_dir: join_paths(source_root, 'src'),
    main_xml: 'appstream-docs.xml',
    ignore_headers: aslib_priv_headers,
    install: true,
    install_dir: join_paths(get_option('prefix'), as_api_doc_target_dir), # requires an absolute path
    content_files: ['../sources/man/appstreamcli.1.xml']
)

# create symlink, so tools using gtk-doc can find the API documentation, without us
# duplicating the data in two places.
# We hardcore the gtk-doc path here, because gtkdoc_html_dir('appstream') creates a
# wrong path due to a Meson bug at the moment
# This code sucks until Meson gains the install_symlink command proposed by the systemd guys
gtk_doc_root = join_paths(get_option('prefix'), get_option('datadir'), 'gtk-doc', 'html')
meson.add_install_script ('sh', '-c',
                          'mkdir -p $DESTDIR@0@ && ln -sf ../../../@1@ $DESTDIR/@2@'
                          .format(gtk_doc_root,
                                  as_api_doc_target_dir,
                                  join_paths(gtk_doc_root, 'appstream'))
)
