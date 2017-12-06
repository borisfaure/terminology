if get_option('nls')
    i18n = import('i18n')
    i18n.gettext('terminology',
            languages: [
                'ca',
                'de',
                'el',
                'eo',
                'es',
                'fi',
                'fr',
                'hi',
                'it',
                'ko',
                'ms',
                'sl',
                'sr',
                'tr',
                'vi'
                ],
            args: [
                '--keyword=_',
                '--keyword=d_:1',
                '--keyword=P_:1,2',
                '--keyword=dP_:1,2',
                '--keyword=N_',
                '--keyword=NP_:1,2',
                '--from-code=UTF-8',
                '--foreign-user'
                ])
  config_data.set('HAVE_GETTEXT', '1')
  config_data.set('ENABLE_NLS', '1')
endif
