icon_sed = 's~%ICON_PATH%~@0@~'.format(join_paths(get_option('prefix'),
                                                  get_option('datadir'),
                                                  'icons/hicolor/128x128/apps'))

custom_target(
  'terminology.desktop',
  input : 'terminology.desktop.in',
  output: 'terminology.desktop',
  command : [sed, icon_sed, '@INPUT@'],
  capture : true,
  install : true,
  install_dir : join_paths(get_option('datadir'), 'applications'))
