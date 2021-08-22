
itconf = configuration_data()
itconf.set('bindir', join_paths(get_option('prefix'),
                              get_option('bindir')))
itconf.set('datadir', join_paths(get_option('prefix'),
                               get_option('datadir')))

configure_file(
    input : 'metainfo-validate.test.in',
    output : 'metainfo-validate.test',
    configuration : itconf,
    install: true,
    install_dir: join_paths('share', 'installed-tests', 'appstream'),
)
