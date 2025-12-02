obpanel_cargs = common_defines + ['-DG_LOG_DOMAIN="Obpanel"']
obpanel_deps = [glib_dep, x11_dep, cairo_dep]
if alsa_dep.found()
  obpanel_cargs += ['-DHAVE_ALSA']
  obpanel_deps += alsa_dep
endif

executable(
  'obpanel',
  'obpanel.c',
  include_directories: [common_includes],
  c_args: obpanel_cargs,
  dependencies: obpanel_deps,
  link_with: libobt,
  install: true)
