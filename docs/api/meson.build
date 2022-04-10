# Meson definition for AppStream API Documentation

as_api_doc_target_dir = join_paths(as_doc_target_dir, 'html', 'api')

glib.gtkdoc (
    'appstream',
    src_dir: join_paths(source_root, 'src'),
    main_xml: 'appstream-docs.xml',
    ignore_headers: aslib_priv_headers,
    install: true,
    install_dir: join_paths(get_option('prefix'), as_api_doc_target_dir), # requires an absolute path
    content_files: ['../xml/man/appstreamcli.1.xml',
                    '../xml/man/appstreamcli-compose.1.xml'
    ]
)

# create symlink, so tools using gtk-doc can find the API documentation, without us
# duplicating the data in two places.
# We hardcore the gtk-doc path here, because gtkdoc_html_dir('appstream') creates a
# wrong path due to a Meson bug at the moment
gtk_doc_root = join_paths(get_option('prefix'), get_option('datadir'), 'gtk-doc', 'html')
# we need to install the empty dir, as the gtkdoc generation happens last
# and a symlink target must exist first.
install_emptydir(join_paths(get_option('prefix'), as_api_doc_target_dir))
install_symlink(
    'appstream',
    pointing_to: '..' / '..' / '..' / as_api_doc_target_dir,
    install_dir: gtk_doc_root
)

#
# Build API documentation for libappstream-compose,
# if the library is enabled.
#
if get_option('compose')
    subdir('compose/')
endif
