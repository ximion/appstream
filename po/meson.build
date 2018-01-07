
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

run_target ('make-linguas',
    command: ['sh',
              join_paths(meson.source_root(), 'contrib/meson/update-linguas.sh')]
)

# maintainer shortcut for updating l10n data
run_target ('l10n-update',
    command: ['ninja', '-C', meson.build_root(),
              'make-linguas',
              as_gettext_domain + '-pot',
              as_gettext_domain + '-update-po']
)
