project('terminology', 'c',
        version: '1.0.99',
        default_options: ['c_std=gnu99'],
        license: 'BSD')

spec_data = configuration_data()
spec_data.set('PACKAGE', meson.project_name())
spec_data.set('VERSION', meson.project_version())

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
config_data.set('_GNU_SOURCE', 1)
config_data.set('__EXTENSIONS__', 1)
config_data.set('_POSIX_PTHREAD_SEMANTICS', 1)
config_data.set('_TANDEM_SOURCE', 1)
config_data.set('_ALL_SOURCE', 1)
config_data.set('_POSIX_SOURCE', 1)
config_data.set('_POSIX_1_SOURCE', 1)

efl_version = '1.8.0'
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
use_translations = false
intl_lib = cc.find_library('intl', required: false)
if intl_lib.found()
  config_data.set('HAVE_GETTEXT', 1)
  config_data.set('ENABLE_NLS', 1)
  terminology_dependencies += [intl_lib]
  use_translations = true
else
  gettext_code = '''
  #include <libintl.h>
  int main(int argc, char *argv[]) {
      (void)ngettext("", "", 0);
      return 0;
  }
  '''
  if cc.links(gettext_code)
    config_data.set('HAVE_GETTEXT', 1)
    config_data.set('ENABLE_NLS', 1)
    use_translations = true
  endif
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
int main(int argc, char *argv[]) { ecore_con_url_head(NULL); return 0; }
'''

elementary_teamwork_code = '''#include <Elementary.h>
int main(int argc, char *argv[]) { elm_win_teamwork_uri_show(NULL); return 0; }
'''

found = 'Not found'

if cc.links(url_head_code, dependencies: terminology_dependencies)
  config_data.set('HAVE_ECORE_CON_URL_HEAD', 1)
  found = 'Found'
endif

message('Checking for ecore_con_url_head: ' + found)

if cc.links(elementary_teamwork_code, dependencies: terminology_dependencies)
  config_dat.set('HAVE_ELM_WIN_TEAMWORK', 1)
  found = 'Found'
else
  found = 'Not Found'
endif

message('Checking for elm_win_teamwork_uri_show: ' + found)

edje_cc = get_option('edje-cc')

if edje_cc == ''
  edje_cc = edje_cc_path
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

configure_file(input: 'terminology.spec.in',
               output: 'terminology.spec',
               configuration: spec_data)

configure_file(input: join_paths('pkgbuild', 'PKGBUILD.in'),
               output: 'PKGBUILD',
               configuration: spec_data)

config_dir = include_directories('.')
subdir('data')
subdir('man')
if use_translations
  subdir('po')
endif
subdir('src/bin')

meson.add_install_script('meson_install_themes.sh', edj_files)
