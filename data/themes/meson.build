command = [edje_cc,
           edje_offscale,
           '-id', join_paths(meson.source_root(), 'data', 'themes', 'images'),
           '-fd', join_paths(meson.source_root(), 'data', 'fonts'),
           '-sd', join_paths(meson.source_root(), 'data', 'themes', 'sounds'),
           '@INPUT@',
           '@OUTPUT@']

edje_to_compile = [
  ['default', ['default_colors.in.edc']],
  ['mild', ['default_colors.in.edc']],
  ['black', ['default_colors.in.edc', 'mild.edc']],
  ['solarized', ['default_colors.in.edc', 'default.edc']],
  ['solarized_light', ['default_colors.in.edc', 'default.edc']],
  ['mustang',['default_colors.in.edc', 'mild.edc']],
  ['base16_ocean_dark', ['default_colors.in.edc']],
  ['smyck', ['default_colors.in.edc', 'mild.edc']],
  ['nord', ['default_colors.in.edc', 'default.edc']],
]

edje_install_dir = join_paths(get_option('datadir'),
                              meson.project_name(), 'themes')

foreach to_compile: edje_to_compile
  out = to_compile[0] + '.edj'
  in = to_compile[0] + '.edc'
  edj_targets += [custom_target(out,
                                install:true,
                                install_dir: edje_install_dir,
                                depend_files: files(to_compile[1]),
                                command: command,
                                input: in,
                                output: out)]
  edj_files += [join_paths(edje_install_dir, out)]
endforeach

subdir('nyanology')
