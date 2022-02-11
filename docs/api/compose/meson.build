# Meson definition for AppStream Compose API Documentation

#
# Build API documentation for libappstream-compose
#

as_composeapi_doc_target_dir = join_paths(as_doc_target_dir, 'html', 'compose-api')
glib.gtkdoc (
    'appstream-compose',
    src_dir: join_paths(source_root, 'compose'),
    main_xml: 'appstream-compose-docs.xml',
    ignore_headers: ascompose_priv_headers,
    install: true,
    install_dir: join_paths(get_option('prefix'), as_composeapi_doc_target_dir) # requires an absolute path
)
meson.add_install_script('sh', '-c',
                         'mkdir -p $DESTDIR@0@ && ln -sf ../../../@1@ $DESTDIR/@2@'
                         .format(gtk_doc_root,
                                 as_composeapi_doc_target_dir,
                                 join_paths(gtk_doc_root, 'appstream-compose'))
)
