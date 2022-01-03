colorschemes = [
  'Belafonte Day',
  'Belafonte Night',
  'Black',
  'Cobalt2',
  'Default',
  'Dracula',
  'Fahrenheit',
  'Material',
  'Mild',
  'Mustang',
  'Nord',
  'Ocean Dark',
  'One Dark',
  'PaleNight',
  'PaperColor',
  'Smyck',
  'Soft Era',
  'Solarized',
  'Solarized Light',
  'Tango Dark',
  'Tango Light',
  'Tomorrow Night Burns',
]

add_color_scheme_sh = find_program(
  'add_color_scheme.sh',
  native: false,
  required: true,
  dirs: [join_paths(meson.source_root(), 'data', 'colorschemes')])

cs_builder = [add_color_scheme_sh,
              eet_bin, '@OUTPUT@', '@INPUT@']

cs_install_dir = join_paths(get_option('datadir'),
                              meson.project_name())

cs_install_dir = join_paths(
  get_option('datadir'),
  meson.project_name(),
  'colorschemes')


foreach c : colorschemes
  custom_target(c,
    install:true,
    install_dir: cs_install_dir,
    install_mode: 'rw-r--r--',
    depend_files : ['ini2desc.py', 'get_name.py', 'add_color_scheme.sh'],
    command: cs_builder,
    input: [c + '.ini'],
    output: [c + '.eet'])
endforeach
