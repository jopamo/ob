project('openbox', 'c',
        version: '3.6',
        default_options: ['warning_level=2', 'c_std=c17'])

pkgconfig = import('pkgconfig')

cc = meson.get_compiler('c')
no_warn_args = cc.get_supported_arguments(['-Wno-unused-parameter'])
add_project_arguments(no_warn_args, language: 'c')

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
prefix = get_option('prefix')
bindir_rel = get_option('bindir')
libdir_rel = get_option('libdir')
datadir_rel = get_option('datadir')
libexecdir_rel = get_option('libexecdir')
sysconfdir_rel = get_option('sysconfdir')
mandir_rel = get_option('mandir')
docdir_rel = join_paths(datadir_rel, 'doc', meson.project_name())

configdir_rel = join_paths(sysconfdir_rel, 'xdg')
rcdir_rel = join_paths(configdir_rel, 'openbox')
themedir_rel = join_paths(datadir_rel, 'themes')
pixmapdir_rel = join_paths(datadir_rel, 'pixmaps')
appsdir_rel = join_paths(datadir_rel, 'applications')
xsessionsdir_rel = join_paths(datadir_rel, 'xsessions')
docxbm_dir_rel = join_paths(docdir_rel, 'xbm')
xsddir_rel = join_paths(datadir_rel, 'openbox')

datadir_abs = join_paths(prefix, datadir_rel)
libexecdir_abs = join_paths(prefix, libexecdir_rel)
bindir_abs = join_paths(prefix, bindir_rel)
configdir_abs = join_paths(prefix, configdir_rel)
rcdir_abs = join_paths(prefix, rcdir_rel)
localedir_abs = join_paths(datadir_abs, 'locale')

# ---------------------------------------------------------------------------
# Versioning (mirrors Autotools setup)
# ---------------------------------------------------------------------------
rr_major_version = 3
rr_minor_version = 6
rr_micro_version = 31
rr_interface_age = 2
rr_binary_age = 2
rr_version = '@0@.@1@'.format(rr_major_version, rr_minor_version)
rr_current = rr_micro_version - rr_interface_age
rr_revision = rr_interface_age
rr_age = rr_binary_age - rr_interface_age
rr_current_minus_age = rr_current - rr_age

obt_major_version = 3
obt_minor_version = 6
obt_micro_version = 4
obt_interface_age = 2
obt_binary_age = 2
obt_version = '@0@.@1@'.format(obt_major_version, obt_minor_version)
obt_current = obt_micro_version - obt_interface_age
obt_revision = obt_interface_age
obt_age = obt_binary_age - obt_interface_age
obt_current_minus_age = obt_current - obt_age

obrender_api_subdir = 'openbox/@0@/obrender'.format(rr_version)
obt_api_subdir = 'openbox/@0@/obt'.format(obt_version)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
glib_dep = dependency('glib-2.0', version: '>=2.14.0')
pango_dep = dependency('pango', version: '>=1.8.0')
pangoxft_dep = dependency('pangoxft', version: '>=1.8.0')
yaml_dep = dependency('yaml-0.1')
x11_dep = dependency('x11')
x11_xcb_dep = dependency('x11-xcb')
xcb_dep = dependency('xcb')
xcb_icccm_dep = dependency('xcb-icccm', required: true)
xcb_shape_dep = dependency('xcb-shape', required: true)
xcb_sync_dep = dependency('xcb-sync', required: true)
xext_dep = dependency('xext')
xrender_dep = dependency('xrender')
xau_dep = dependency('xau')
intl_dep = cc.find_library('intl', required: false)

# Optional dependencies / features
startup_opt = get_option('startup_notification')
libsn_dep = dependency('libstartup-notification-1.0',
                       required: startup_opt.enabled())

xcursor_opt = get_option('xcursor')
xcursor_dep = dependency('xcursor', required: xcursor_opt.enabled())

imlib_opt = get_option('imlib2')
imlib_dep = dependency('imlib2', required: imlib_opt.enabled())

librsvg_opt = get_option('librsvg')
librsvg_dep = dependency('librsvg-2.0', required: librsvg_opt.enabled())

xrandr_dep = dependency('xrandr', required: true)

have_xshape = cc.has_header('X11/extensions/shape.h', dependencies: xext_dep)
if not have_xshape
  error('XShape support requires X11/extensions/shape.h')
endif

xdamage_opt = get_option('xdamage')
xdamage_dep = dependency('xdamage', required: xdamage_opt.enabled())
have_xdamage = false
if not xdamage_opt.disabled()
  have_xdamage = xdamage_dep.found()
  if have_xdamage
    have_xdamage = cc.has_header('X11/extensions/Xdamage.h',
                                 dependencies: [x11_dep, xdamage_dep])
  endif
  if not have_xdamage and xdamage_opt.enabled()
    error('XDamage support requested but X11/extensions/Xdamage.h not found')
  endif
endif

have_xsync = cc.has_header('X11/extensions/sync.h', dependencies: xext_dep)
if not have_xsync
  error('XSync support requires X11/extensions/sync.h')
endif

have_xkb = cc.has_function('XkbBell', dependencies: x11_dep)
if not have_xkb
  error('XKB support requested but libX11 does not provide XkbBell()')
endif
xkbcommon_dep = dependency('xkbcommon', required: true)
xkbcommon_deps = [xkbcommon_dep]

session_opt = get_option('session_management')
sm_deps = []
have_session = false
if not session_opt.disabled()
  sm_lib = cc.find_library('SM', required: session_opt.enabled())
  ice_lib = cc.find_library('ICE', required: session_opt.enabled())
  have_session = sm_lib.found() and ice_lib.found()
  if have_session
    sm_deps = [sm_lib, ice_lib]
  elif session_opt.enabled()
    error('Session management requested but libSM/libICE not found')
  endif
endif

have_libsn = (not startup_opt.disabled()) and libsn_dep.found()
have_xcursor = false
if not xcursor_opt.disabled()
  have_xcursor = xcursor_dep.found()
  if have_xcursor
    have_xcursor = cc.has_header(
      'X11/Xcursor/Xcursor.h',
      dependencies: [x11_dep, xcursor_dep])
  endif
  if not have_xcursor and xcursor_opt.enabled()
    error('Xcursor support requested but X11/Xcursor/Xcursor.h not found')
  endif
endif
have_imlib = (not imlib_opt.disabled()) and imlib_dep.found()
have_librsvg = (not librsvg_opt.disabled()) and librsvg_dep.found()
have_xrandr = xrandr_dep.found()

# ---------------------------------------------------------------------------
# Common compile helpers
# ---------------------------------------------------------------------------
common_includes = include_directories('.')
theme_name = get_option('default_theme')

common_defines = [
  '-DLOCALEDIR="@0@"'.format(localedir_abs),
  '-DDATADIR="@0@"'.format(datadir_abs),
  '-DCONFIGDIR="@0@"'.format(configdir_abs),
  '-DPACKAGE_NAME="openbox"',
  '-DPACKAGE_TARNAME="openbox"',
  '-DPACKAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DPACKAGE_STRING="openbox @0@"'.format(meson.project_version()),
  '-DPACKAGE_BUGREPORT="http://bugzilla.icculus.org"',
  '-DPACKAGE_URL=""',
  '-DPACKAGE="openbox"',
  '-DVERSION="@0@"'.format(meson.project_version()),
]

header_checks = {
  'ctype.h': 'HAVE_CTYPE_H',
  'dirent.h': 'HAVE_DIRENT_H',
  'errno.h': 'HAVE_ERRNO_H',
  'fcntl.h': 'HAVE_FCNTL_H',
  'grp.h': 'HAVE_GRP_H',
  'locale.h': 'HAVE_LOCALE_H',
  'pwd.h': 'HAVE_PWD_H',
  'signal.h': 'HAVE_SIGNAL_H',
  'stdio.h': 'HAVE_STDIO_H',
  'stdlib.h': 'HAVE_STDLIB_H',
  'string.h': 'HAVE_STRING_H',
  'strings.h': 'HAVE_STRINGS_H',
  'sys/select.h': 'HAVE_SYS_SELECT_H',
  'sys/socket.h': 'HAVE_SYS_SOCKET_H',
  'sys/stat.h': 'HAVE_SYS_STAT_H',
  'sys/time.h': 'HAVE_SYS_TIME_H',
  'sys/types.h': 'HAVE_SYS_TYPES_H',
  'sys/wait.h': 'HAVE_SYS_WAIT_H',
  'unistd.h': 'HAVE_UNISTD_H',
}
foreach header, define_name : header_checks
  if cc.has_header(header)
    common_defines += ['-D@0@'.format(define_name)]
  endif
endforeach
feature_defines = []
if have_libsn
  feature_defines += ['-DUSE_LIBSN']
endif
if have_xcursor
  feature_defines += ['-DUSE_XCURSOR']
endif
if have_imlib
  feature_defines += ['-DUSE_IMLIB2']
endif
if have_librsvg
  feature_defines += ['-DUSE_LIBRSVG']
endif
if have_session
  feature_defines += ['-DUSE_SM']
endif
if have_xrandr
  feature_defines += ['-DXRANDR']
endif
if have_xshape
  feature_defines += ['-DSHAPE']
endif
if have_xdamage
  feature_defines += ['-DDAMAGE']
endif
if have_xsync
  feature_defines += ['-DSYNC']
endif

# This header simply carries the project version string.
version_conf = configuration_data()
version_conf.set('OB_VERSION', meson.project_version())
top_version_h = configure_file(
  input: 'version.h.in',
  output: 'version.h',
  configuration: version_conf)

# ---------------------------------------------------------------------------
# Build subdirectories
# ---------------------------------------------------------------------------
subdir('openbox/x11')
subdir('obt')
subdir('obrender')
subdir('openbox')
subdir('tools')
subdir('tests')
subdir('data')
subdir('themes')
subdir('doc')
subdir('po')

# ---------------------------------------------------------------------------
# Install pkg-config files for the public libraries
# ---------------------------------------------------------------------------
obt_private_libs = [x11_dep, x11_xcb_dep, xcb_dep, xext_dep, xrender_dep, glib_dep]
obt_private_libs += xrandr_dep
if have_xdamage
  obt_private_libs += xdamage_dep
endif

pkgconfig.generate(
  name: 'Obt',
  filebase: 'obt-3.5',
  description: 'Openbox Toolkit Library',
  version: obt_version,
  subdirs: obt_api_subdir,
  libraries: libobt,
  libraries_private: obt_private_libs,
  requires: ['glib-2.0'])

obrender_requires = ['obt-3.5', 'glib-2.0', 'xft', 'pangoxft']
if have_imlib
  obrender_requires += ['imlib2']
endif
if have_librsvg
  obrender_requires += ['librsvg-2.0']
endif

obrender_private_libs = [
  x11_dep, xext_dep, xrender_dep, glib_dep,
  pango_dep, pangoxft_dep,
]
if have_imlib
  obrender_private_libs += imlib_dep
endif
if have_librsvg
  obrender_private_libs += librsvg_dep
endif

pkgconfig.generate(
  name: 'ObRender',
  filebase: 'obrender-3.5',
  description: 'Openbox Render Library',
  version: rr_version,
  subdirs: obrender_api_subdir,
  libraries: libobrender,
  libraries_private: obrender_private_libs,
  requires: obrender_requires)

# ---------------------------------------------------------------------------
# Extra documentation that used to come from dist_doc_DATA
# ---------------------------------------------------------------------------
install_data(
  [
    'HACKING.md',
    'COPYING',
  ],
  install_dir: docdir_rel)

# ---------------------------------------------------------------------------
# Summary output
# ---------------------------------------------------------------------------
summary({
  'Startup notification': have_libsn,
  'Xcursor': have_xcursor,
  'Imlib2 icons': have_imlib,
  'SVG icons (librsvg)': have_librsvg,
  'XRandR': have_xrandr,
  'XShape': have_xshape,
  'XSync': have_xsync,
  'XKB': have_xkb,
  'Session management': have_session,
}, bool_yn: true, section: 'Optional features')
