obx11_sources = files('x11.c')

obx11 = static_library(
  'obx11helpers',
  obx11_sources,
  include_directories: [common_includes],
  c_args: common_defines + feature_defines + ['-DG_LOG_DOMAIN="ObX11"'],
  dependencies: [glib_dep, x11_dep, x11_xcb_dep, xcb_dep],
  pic: true,
  install: false)

obx11_dep = declare_dependency(
  link_with: obx11,
  include_directories: [common_includes],
  dependencies: [glib_dep, x11_dep, x11_xcb_dep, xcb_dep],
)
