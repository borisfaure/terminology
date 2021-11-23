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

colorschemes_out = []

foreach c : colorschemes_desc
  colorschemes_out += c.replace('ini','eet')
endforeach

cs_builder = [join_paths(meson.source_root(),
                         'data', 'colorschemes', 'builder.sh'),
              eet_bin, '@INPUT@']
cs_install_dir = join_paths(get_option('datadir'),
                              meson.project_name(),
                              'colorschemes')


custom_target('colorschemes',
  install:true,
  install_dir: cs_install_dir,
  install_mode: 'rw-r--r--',
  depend_files: ['builder.sh', 'ini2desc.py', 'get_name.py', 'add_color_scheme.sh'],
  command: cs_builder,
  input: colorschemes_desc,
  output: colorschemes_out)
