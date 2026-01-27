# Meson definition for AppStream contrib

if get_option('vapi') and get_option('gir')
    subdir('vapi/')
endif
if get_option('apt-support')
    subdir('apt-conf/')
endif

if get_option('bash-completion')
  bash_completion_dep = dependency('bash-completion', version: '>=2.0')
  install_data(
    'appstreamcli.bash-completion',
    rename: 'appstreamcli',
    install_dir: bash_completion_dep.get_variable(
                    pkgconfig: 'completionsdir',
                    pkgconfig_define: [
                        'prefix', get_option('prefix'),
                        'datadir', get_option('prefix') / get_option('datadir')
                    ])
  )
endif
