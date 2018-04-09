project('terminology', 'c',
        version: '1.2.0',
        default_options: ['c_std=gnu99'],
        license: 'BSD')

cc = meson.get_compiler('c')

prefix = get_option('prefix')
add_global_arguments('-DHAVE_CONFIG_H=1', language: 'c')
config_data = configuration_data()
config_data.set('EFL_BETA_API_SUPPORT', 1)
config_data.set('EFL_EO_API_SUPPORT', 1)
config_data.set_quoted('PACKAGE_VERSION', meson.project_version())
config_data.set_quoted('PACKAGE', meson.project_name())
config_data.set_quoted('PACKAGE_BUGREPORT',
                       'enlightenment-devel@lists.sourceforge.net')
config_data.set_quoted('PACKAGE_NAME', meson.project_name())
config_data.set_quoted('PACKAGE_TARNAME', meson.project_name())
config_data.set_quoted('PACKAGE_URL',
                       'https://www.enlightenment.org/about-terminology')
config_data.set_quoted('PACKAGE_BIN_DIR',
                       join_paths(prefix, get_option('bindir')))
config_data.set_quoted('PACKAGE_DATA_DIR',
                       join_paths(prefix, get_option('datadir'),
                                  meson.project_name()))
config_data.set_quoted('PACKAGE_LIB_DIR',
                       join_paths(prefix, get_option('libdir')))
config_data.set_quoted('LOCALEDIR', join_paths(prefix, get_option('localedir')))

host_os = host_machine.system()

if host_os == 'linux'
   config_data.set('_GNU_SOURCE', 1)
   config_data.set('__EXTENSIONS__', 1)
   config_data.set('_POSIX_PTHREAD_SEMANTICS', 1)
   config_data.set('_TANDEM_SOURCE', 1)
   config_data.set('_ALL_SOURCE', 1)
   config_data.set('_POSIX_SOURCE', 1)
   config_data.set('_POSIX_1_SOURCE', 1)
endif

efl_version = '1.20.0'
efl_deps = ['edje',
            'elementary',
            'eina',
            'eet',
            'evas',
            'ecore',
            'ecore-evas',
            'ecore-file',
            'emotion',
            'ecore-input',
            'ecore-imf',
            'ecore-imf-evas',
            'ecore-ipc',
            'efreet',
            'ecore-con',
            'ethumb_client']
terminology_dependencies = []
edje_cc_path = ''
edj_targets = []
edj_files = []

if get_option('nls') == true
  subdir('po')
  terminology_dependencies += cc.find_library('intl', required: false)
endif

foreach efl_dep: efl_deps
  dep = dependency(efl_dep, version: '>=' + efl_version)
  terminology_dependencies += [dep]
  if efl_dep == 'edje'
    edje_cc_path = dep.get_pkgconfig_variable('prefix') + '/bin/edje_cc'
  endif
endforeach

if cc.has_function('mkstemps')
  config_data.set('HAVE_MKSTEMPS', 1)
endif

url_head_code = '''#include <Ecore_Con.h>
int main(void) { ecore_con_url_head(NULL); return 0; }
'''

found = 'Not found'

if cc.links(url_head_code, dependencies: terminology_dependencies)
  config_data.set('HAVE_ECORE_CON_URL_HEAD', 1)
  found = 'Found'
endif

message('Checking for ecore_con_url_head: ' + found)

edje_cc = get_option('edje-cc')

if edje_cc == ''
  edje_cc = edje_cc_path
endif

res = run_command(edje_cc, meson.current_source_dir() + '/data/test_offscale.edc')
if res.returncode() == 0
  message('edje_cc has support for offscale')
  edje_offscale='-DHAS_OFFSCALE=1'
else
  message('edje_cc does not have support for offscale')
  edje_offscale='-DHAS_OFFSCALE=0'
endif

fuzzing = get_option('fuzzing')

if fuzzing
  config_data.set('ENABLE_FUZZING', 1)
  message('Fuzzing is enabled')
else
  message('Fuzzing is disabled')
endif

message('edje_cc set to:' + edje_cc)

configure_file(output: 'terminology_config.h',
               configuration: config_data)

config_dir = include_directories('.')
subdir('data')
subdir('man')
subdir('src/bin')

meson.add_install_script('meson_install_themes.sh', edj_files)
