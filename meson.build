project('terminology', 'c',
        version: '1.14.0',
        default_options: ['buildtype=plain', 'c_std=gnu99'],
        license: 'BSD',
        meson_version: '>=0.59.0')

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

efl_version = '1.26.0'
m_dep = cc.find_library('m', required : false)
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
terminology_dependencies = [ m_dep ]
edje_cc_path = ''
eet_path = ''
edj_targets = []
edj_files = []

if get_option('nls') == true
  subdir('po')
  # need both cause libintl may be included in glibc or not
  dep = dependency('intl', required: false)
  if dep.found()
    terminology_dependencies += dep
  else
    terminology_dependencies += cc.find_library('intl', required: false)
  endif
endif

foreach efl_dep: efl_deps
  dep = dependency(efl_dep, version: '>=' + efl_version)
  terminology_dependencies += [dep]
  if efl_dep == 'edje'
    edje_cc_path = dep.get_variable(pkgconfig: 'prefix') + '/bin/edje_cc'
  endif
  if efl_dep == 'eet'
    eet_path = dep.get_variable(pkgconfig: 'prefix') + '/bin/eet'
  endif
endforeach

if cc.has_function('mkstemps')
  config_data.set('HAVE_MKSTEMPS', 1)
endif

if cc.has_function('strchrnul')
  config_data.set('HAVE_STRCHRNUL', 1)
endif

sel_format_urilist_code = '''#include <Elementary.h>
int main(void) { return ELM_SEL_FORMAT_URILIST; }
'''

found = 'Not found'

if cc.links(sel_format_urilist_code, dependencies: terminology_dependencies)
  config_data.set('HAVE_ELM_SEL_FORMAT_URILIST', 1)
  found = 'Found'
endif

message('Checking for ELM_SEL_FORMAT_URILIST: ' + found)

eet_bin = get_option('eet')
if eet_bin == ''
  eet_bin = eet_path
endif

edje_cc = get_option('edje-cc')
if edje_cc == ''
  edje_cc = edje_cc_path
endif

res = run_command(edje_cc, meson.current_source_dir() + '/data/test_offscale.edc', check: false)
if res.returncode() == 0
  message('edje_cc has support for offscale')
  edje_offscale='-DHAS_OFFSCALE=1'
else
  message('edje_cc does not have support for offscale')
  edje_offscale='-DHAS_OFFSCALE=0'
endif

fuzzing = get_option('fuzzing')

if fuzzing
  message('Fuzzing is enabled')
else
  message('Fuzzing is disabled')
endif

tests = get_option('tests')
if tests
  message('Tests are enabled')
else
  message('Tests are disabled')
endif

message('edje_cc set to:' + edje_cc)

sed = find_program('sed')

configure_file(output: 'terminology_config.h',
               configuration: config_data)

config_dir = include_directories('.')
subdir('data')
subdir('man')
subdir('src/bin')

