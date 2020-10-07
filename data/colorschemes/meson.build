colorschemes_desc = [
  'Mild.ini',
  'Mustang.ini',
  'Nord.ini',
  'Ocean Dark.ini',
  'PaperColor.ini',
  'Smyck.ini',
  'Solarized.ini',
  'Solarized Light.ini',
]
cs_builder = [join_paths(meson.source_root(),
                         'data', 'colorschemes', 'builder.sh'),
              eet_bin, '@OUTPUT@', '@INPUT@']
cs_install_dir = join_paths(get_option('datadir'),
                              meson.project_name())


custom_target('colorschemes.eet',
  install:true,
  install_dir: cs_install_dir,
  install_mode: 'rw-r--r--',
  depend_files: ['builder.sh', 'ini2desc.py'],
  command: cs_builder,
  input: colorschemes_desc,
  output: 'colorschemes.eet')
