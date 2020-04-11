if get_option('nls')
    i18n = import('i18n')
    # define GETTEXT_PACKAGE
    add_project_arguments('-DGETTEXT_PACKAGE="terminology"', language:'c')
    i18n.gettext(meson.project_name(),
          args: [
            '--directory=' + meson.source_root(),
            '--keyword=_',
            '--keyword=d_:1',
            '--keyword=P_:1,2',
            '--keyword=dP_:1,2',
            '--keyword=N_',
            '--keyword=NP_:1,2',
            '--from-code=UTF-8',
            '--foreign-user'
            ])
  config_data.set('ENABLE_NLS', '1')
endif
