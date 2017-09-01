
as_gettext_domain = 'appstream'
i18n.gettext (as_gettext_domain,
    preset : 'glib',
    data_dirs: ['../data/its'],
    args: [
        '--default-domain=' + as_gettext_domain,
        '--from-code=UTF-8',
        '-i', '-s', '--no-wrap', '-c',
        '--package-name=' + as_gettext_domain,
        '--copyright-holder=Matthias Klumpp',
        '--msgid-bugs-address=appstream@lists.freedesktop.org'
    ]
)

make_linguas = custom_target ('make-linguas',
    build_always: true,
    output: 'LINGUAS_dummy', # fake output
    command: [
        'sh', '-c',
        'find ' + meson.current_source_dir() +
        ' -type f' +
        ' -iname "*.po"' +
        ' -printf \'%f\\n\'' +
        ' | grep -oP \'.*(?=[.])\' | sort' +
        ' > ' + join_paths(meson.current_source_dir(), 'LINGUAS')
    ]
)

# maintainer shortcut for updating l10n data
run_target ('l10n-update',
    depends: make_linguas,
    command: ['ninja', '-C', meson.build_root(),
              as_gettext_domain + '-pot',
              as_gettext_domain + '-update-po']
)
