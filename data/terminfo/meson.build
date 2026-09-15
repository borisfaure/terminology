tic = find_program('tic', native: true, required: get_option('terminfo'))

install_data('terminology.terminfo',
             install_dir: join_paths(get_option('datadir'),
                                     meson.project_name(), 'terminfo'))

if tic.found()
  # tic lays out <first letter>/<name>
  custom_target('terminfo',
    input: 'terminology.terminfo',
    output: 't',
    command: [tic, '-x', '-o', '@OUTDIR@', '@INPUT@'],
    install: true,
    install_dir: join_paths(get_option('datadir'), 'terminfo'))
endif
