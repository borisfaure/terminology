colorschemes_desc = [
  'Belafonte Day.ini',
  'Belafonte Night.ini',
  'Black.ini',
  'Cobalt2.ini',
  'Dracula.ini',
  'Fahrenheit.ini',
  'Material.ini',
  'Mild.ini',
  'Mustang.ini',
  'Nord.ini',
  'Ocean Dark.ini',
  'One Dark.ini',
  'PaleNight.ini',
  'PaperColor.ini',
  'Smyck.ini',
  'Soft Era.ini',
  'Solarized.ini',
  'Solarized Light.ini',
  'Tango Dark.ini',
  'Tango Light.ini',
  'Tomorrow Night Burns.ini',
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
  depend_files: ['builder.sh', 'ini2desc.py', 'get_name.py', 'add_color_scheme.sh'],
  command: cs_builder,
  input: colorschemes_desc,
  output: 'colorschemes.eet')
