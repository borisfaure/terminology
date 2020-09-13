colorschemes_desc = [
  'Nord.desc',
  'PaperColor.desc',
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
  command: cs_builder,
  input: colorschemes_desc,
  output: 'colorschemes.eet')
