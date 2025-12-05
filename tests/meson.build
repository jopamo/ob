test_sources = [
  'borderchange.c',
  'confignotifymax.c',
  'fakeunmap.c',
  'mingrow.c',
  'noresize.c',
  'showhide.c',
  'usertimewin.c',
  'icons.c',
]



test_deps = [x11_dep, x11_xcb_dep, xcb_dep, xext_dep, glib_dep]
test_targets = []
foreach src : test_sources
  name = '@0@-@1@'.format('test', src.split('.')[0])
  exe = executable(
    name,
    src,
    dependencies: test_deps,
    install: false,
    build_by_default: false)
  test_targets += exe
endforeach

hidpi_test = executable(
  'test-hidpi',
  'hidpi.c',
  include_directories: [common_includes],
  dependencies: [glib_dep, pango_dep, pangoxft_dep, x11_dep, xext_dep, xrender_dep],
  link_with: [libobrender, libobt],
  install: false,
  build_by_default: false)
test_targets += hidpi_test

if test_targets.length() > 0
  alias_target('x11-regression-tests', test_targets)
endif

test_timeout_s = 10
test_runner = configure_file(
  input: 'run-x11-regressions.sh.in',
  output: 'run-x11-regressions.sh',
  configuration: {
    'TEST_BUILD_DIR': meson.current_build_dir(),
    'TEST_TIMEOUT_S': test_timeout_s,
    'OPENBOX_BIN': join_paths(meson.project_build_root(), 'openbox', 'openbox'),
  })

sh = find_program('sh')
test(
  'x11-regressions',
  sh,
  args: [test_runner],
  workdir: meson.current_build_dir(),
  suite: ['x11'],
  timeout: 1200,
  is_parallel: false,
  depends: test_targets)

unit_prop_test = executable(
  'test-unit-prop',
  'unit-prop.c',
  include_directories: [common_includes],
  dependencies: [glib_dep, obx11_dep],
  link_with: [libobt],
  install: false,
  build_by_default: false)

test(
  'unit-prop',
  unit_prop_test,
  suite: ['unit'],
  timeout: 30,
  is_parallel: true)
