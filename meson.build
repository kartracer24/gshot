project(
  'gshot',
  'c',
  version: '41.0',
  license: 'GPLv2+',
  meson_version: '>= 0.59.0',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=1',
    'c_std=c99',
  ]
)

add_project_arguments([
  '-D_GNU_SOURCE',
  '-D_POSIX_C_SOURCE=200809L',
  '-D_XOPEN_SOURCE=700',
  '-D_DEFAULT_SOURCE'
], language: 'c')

cc = meson.get_compiler('c')

gnome_screenshot_prefix = get_option('prefix')
gnome_screenshot_bindir = join_paths(gnome_screenshot_prefix, get_option('bindir'))
gnome_screenshot_datadir = join_paths(gnome_screenshot_prefix, get_option('datadir'))
gnome_screenshot_libexecdir = join_paths(gnome_screenshot_prefix, get_option('libexecdir'))
gnome_screenshot_localedir = join_paths(gnome_screenshot_datadir, 'locale')
gnome_screenshot_appsdir = join_paths(gnome_screenshot_datadir, 'applications')
gnome_screenshot_appdatadir = join_paths(gnome_screenshot_datadir, 'metainfo')
gnome_screenshot_servicesdir = join_paths(gnome_screenshot_datadir, 'dbus-1', 'services')
gnome_screenshot_schemadir = join_paths(gnome_screenshot_datadir, 'glib-2.0', 'schemas')

glib_req_version = '>= 2.35.1'
gtk_req_version = '>= 4.10.0'

mathlib_dep = cc.find_library('m', required: false)
wayland_client_dep = dependency('wayland-client', required: false)
wayland_protocols_dep = dependency('wayland-protocols', required: false)
glib_dep = dependency('glib-2.0', version: glib_req_version)
gtk_dep = dependency('gtk4', version: gtk_req_version)
gtk_wayland_dep = dependency('gtk4-wayland', version: gtk_req_version)

config_h = configuration_data()
config_h.set_quoted('VERSION', meson.project_version())
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())

has_wayland = wayland_client_dep.found() and wayland_protocols_dep.found()
if has_wayland
  config_h.set('HAVE_WAYLAND', 1)
endif

configure_file(output: 'config.h', configuration: config_h)

root_inc = include_directories('.')

fs = import('fs')
i18n = import('i18n')

subdir('data')
subdir('src')
subdir('po')

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)
