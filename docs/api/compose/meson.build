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

# we need to install the empty dir, as the gtkdoc generation happens last
# and a symlink target must exist first.
install_emptydir(join_paths(get_option('prefix'), as_composeapi_doc_target_dir))
install_symlink(
    'appstream-compose',
    pointing_to: '..' / '..' / '..' / as_composeapi_doc_target_dir,
    install_dir: gtk_doc_root
)
