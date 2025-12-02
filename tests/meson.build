test_sources = [
  'aspect.c',
  'big.c',
  'borderchange.c',
  'confignotify.c',
  'confignotifymax.c',
  'cursorio.c',
  'duplicatesession.c',
  'extentsrequest.c',
  'fakeunmap.c',
  'fallback.c',
  'focusout.c',
  'fullscreen.c',
  'grav.c',
  'groupmodal.c',
  'grouptran.c',
  'grouptran2.c',
  'grouptrancircular.c',
  'grouptrancircular2.c',
  'iconifydelay.c',
  'icons.c',
  'mapiconic.c',
  'mingrow.c',
  'modal.c',
  'modal2.c',
  'modal3.c',
  'noresize.c',
  'oldfullscreen.c',
  'override.c',
  'overrideinputonly.c',
  'positioned.c',
  'resize.c',
  'restack.c',
  'showhide.c',
  'skiptaskbar.c',
  'skiptaskbar2.c',
  'stackabove.c',
  'stacking.c',
  'strut.c',
  'title.c',
  'urgent.c',
  'usertimewin.c',
  'wmhints.c',
]

test_sources += ['shape.c']

test_deps = [x11_dep, xext_dep, glib_dep]
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

test_timeout_s = 5
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
  timeout: 300,
  is_parallel: false,
  depends: test_targets)
