test_sources = [
  'aspect.c',
  'borderchange.c',
  'confignotify.c',
  'confignotifymax.c',
  'fakeunmap.c',
  'iconifydelay.c',
  'mapiconic.c',
  'mingrow.c',
  'modal3.c',
  'noresize.c',
  'oldfullscreen.c',
  'overrideinputonly.c',
  'positioned.c',
  'restack.c',
  'skiptaskbar2.c',
  'stacking.c',
]

if have_xshape
  test_sources += ['shape.c']
endif

test_deps = [x11_dep, xext_dep, glib_dep]
xvfb_run = find_program('xvfb-run', required: false)
xvfb_wrapper_sh = find_program(meson.current_source_dir() / 'xvfb-wrapper.sh', required: false)
xvfb_wrapper_py = find_program(meson.current_source_dir() / 'xvfb-wrapper.py', required: false)
test_targets = []
foreach src : test_sources
  name = '@0@-@1@'.format('test', src.split('.')[0])
  exe = executable(
    name,
    src,
    dependencies: test_deps,
    install: false,
    build_by_default: true)
  test_targets += exe

  # Register test with Xvfb wrapper if available
  if xvfb_wrapper_sh.found()
    test(name, xvfb_wrapper_sh,
         args: ['--auto-servernum', '--wait', '2', '--', exe.full_path()],
         timeout: 30,
         workdir: meson.current_build_dir(),
         is_parallel: false)
  elif xvfb_wrapper_py.found()
    test(name, xvfb_wrapper_py,
         args: ['--wait', '2', '--auto-servernum', '--', exe.full_path()],
         timeout: 30,
         workdir: meson.current_build_dir(),
         is_parallel: false)
  elif xvfb_run.found()
    test(name, xvfb_run,
         args: ['--auto-servernum', '--server-args=-screen 0 1280x800x24', exe.full_path()],
         timeout: 30,
         workdir: meson.current_build_dir(),
         is_parallel: false)
  endif
endforeach

if test_targets.length() > 0
  alias_target('x11-regression-tests', test_targets)
endif
