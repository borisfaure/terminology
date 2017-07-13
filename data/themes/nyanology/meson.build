command = [edje_cc,
           '-id', join_paths(meson.source_root(), 'data', 'themes',
                             'nyanology', 'images'),
           '-id', join_paths(meson.source_root(), 'data', 'themes', 'images'),
           '-fd', join_paths(meson.source_root(), 'data', 'fonts'),
           '-sd', join_paths(meson.source_root(), 'data', 'themes', 'sounds'),
           '@INPUT@',
           '@OUTPUT@']

edj_targets += [custom_target('nyanology.edj',
                              install:true,
                              depend_files: files(['about.edc',
                                                   'nyancursor.edc']),
                              install_dir: edje_install_dir,
                              command: command,
                              input:'nyanology.edc',
                              output:'nyanology.edj')]

edj_files += [join_paths(edje_install_dir, 'nyanology.edj')]
