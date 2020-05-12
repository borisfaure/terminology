command = [edje_cc,
           edje_offscale,
           '-id', join_paths(meson.source_root(), 'data', 'themes', 'images'),
           '-fd', join_paths(meson.source_root(), 'data', 'fonts'),
           '-sd', join_paths(meson.source_root(), 'data', 'themes', 'sounds'),
           '@INPUT@',
           '@OUTPUT@']

default_deps = [
  'default.edc',
  'default/colors.edc',
  'default/keybinding.edc',
  'default/miniview.edc',
  'default/core.edc',
  'default/background.edc',
  'default/tabbar_back.edc',
  'default/base.edc',
  'default/selection.edc',
  'default/cursor.edc',
  'default/sel.edc',
  'default/link.edc',
  'default/media.edc',
  'default/icon.edc',
  'default/about.edc',
  'default/fontpreview.edc',
  'default/tab_drag_thumb.edc',
  'default/color_preview.edc',
  ]
mild_deps = ['mild.edc', 'default/colors.edc']
edje_to_compile = [
  ['default', default_deps],
  ['mild', mild_deps],
  ['black', mild_deps],
  ['solarized', default_deps],
  ['solarized_light', default_deps],
  ['mustang', mild_deps],
  ['base16_ocean_dark', mild_deps],
  ['smyck', mild_deps],
  ['nord', default_deps],
  ['papercolor', default_deps],
]

edje_install_dir = join_paths(get_option('datadir'),
                              meson.project_name(), 'themes')

foreach to_compile: edje_to_compile
  output = to_compile[0] + '.edj'
  input = to_compile[0] + '.edc'
  edj_targets += [custom_target(output,
                                install:true,
                                install_dir: edje_install_dir,
                                install_mode: 'rw-r--r--',
                                depend_files: files(to_compile[1]),
                                command: command,
                                input: input,
                                output: output)]
  edj_files += [join_paths(edje_install_dir, output)]
endforeach

subdir('nyanology')
