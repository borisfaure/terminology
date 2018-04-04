terminology_sources = ['private.h', 'coverity.h',
                       'about.c', 'about.h',
                       'col.c', 'col.h',
                       'config.c', 'config.h',
                       'controls.c', 'controls.h',
                       'ipc.c', 'ipc.h',
                       'keyin.c', 'keyin.h',
                       'main.c', 'main.h',
                       'media.c', 'media.h',
                       'options.c', 'options.h',
                       'options_font.c', 'options_font.h',
                       'options_theme.c', 'options_theme.h',
                       'options_themepv.c', 'options_themepv.h',
                       'options_background.c', 'options_background.h',
                       'options_colors.c', 'options_colors.h',
                       'options_behavior.c', 'options_behavior.h',
                       'options_keys.c', 'options_keys.h',
                       'options_helpers.c', 'options_helpers.h',
                       'options_video.c', 'options_video.h',
                       'options_elm.c', 'options_elm.h',
                       'sel.c', 'sel.h',
                       'miniview.c', 'miniview.h',
                       'termio.c', 'termio.h',
                       'termcmd.c', 'termcmd.h',
                       'term_container.h',
                       'termiolink.c', 'termiolink.h',
                       'termpty.c', 'termpty.h',
                       'termptydbl.c', 'termptydbl.h',
                       'termptyesc.c', 'termptyesc.h',
                       'termptyops.c', 'termptyops.h',
                       'termptygfx.c', 'termptygfx.h',
                       'termptyext.c', 'termptyext.h',
                       'termptysave.c', 'termptysave.h',
                       'lz4/lz4.c', 'lz4/lz4.h',
                       'md5/md5.c', 'md5/md5.h',
                       'utf8.c', 'utf8.h',
                       'win.c', 'win.h',
                       'utils.c', 'utils.h',
                       'extns.c', 'extns.h',
                       'gravatar.c', 'gravatar.h',
                       'tty_keys.h',
                       'sb.c', 'sb.h']

tybg_sources = ['tycommon.c', 'tycommon.h', 'tybg.c']
tyalpha_sources = ['tycommon.c', 'tycommon.h', 'tyalpha.c']
typop_sources = ['tycommon.c', 'tycommon.h', 'typop.c']
tyq_sources = ['tycommon.c', 'tycommon.h', 'tyq.c']
tycat_sources = ['tycommon.c', 'tycommon.h', 'tycat.c', 'extns.c', 'extns.h']
tyls_sources = ['extns.c', 'extns.h', 'tyls.c', 'tycommon.c', 'tycommon.h']
tysend_sources = ['tycommon.c', 'tycommon.h', 'tysend.c']
tyfuzz_sources = ['termptyesc.c', 'termptyesc.h',
                  'termptysave.c', 'termptysave.h',
                  'termptyops.c', 'termptyops.h',
                  'termptydbl.c', 'termptydbl.h',
                  'termptyext.c', 'termptyext.h',
                  'termptygfx.c', 'termptygfx.h',
                  'termpty.c', 'termpty.h',
                  'config.c', 'config.h',
                  'col.c', 'col.h',
                  'tyfuzz.c']

executable('terminology',
           terminology_sources,
           edj_targets,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tybg',
           tybg_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tyalpha',
           tyalpha_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('typop',
           typop_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tyq',
           tyq_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tycat',
           tycat_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tyls',
           tyls_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)
executable('tysend',
           tysend_sources,
           install: true,
           include_directories: config_dir,
           dependencies: terminology_dependencies)

if fuzzing
  executable('tyfuzz',
             tyfuzz_sources,
             install: true,
             include_directories: config_dir,
             dependencies: terminology_dependencies)
endif
