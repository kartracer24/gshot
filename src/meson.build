wayland_scanner = find_program('wayland-scanner')
wayland_protocols_dir = wayland_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')

generated_protocols = [
  'staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml',
  'staging/ext-image-capture-source/ext-image-capture-source-v1.xml',
  'staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml',
]

protocol_sources = []
foreach p : generated_protocols
  xml_path = join_paths(wayland_protocols_dir, p)
  base_name = p.split('/')[-1].split('.xml')[0]

  protocol_sources += custom_target(
    base_name + '-client-protocol[h]',
    input: xml_path,
    output: base_name + '-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )

  protocol_sources += custom_target(
    base_name + '-protocol[c]',
    input: xml_path,
    output: base_name + '-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

sources = [
  'gnome-screenshot.c',

  'cheese-flash.c',

  'screenshot-application.c',
  'screenshot-area-selection.c',
  'screenshot-backend.c',
  'screenshot-backend-shell.c',
  'screenshot-backend-wayland.c',
  'screenshot-config.c',
  'screenshot-dialog.c',
  'screenshot-filename-builder.c',
  'screenshot-interactive-dialog.c',
  'screenshot-utils.c',
] + protocol_sources

include_directories = [
  root_inc,
  include_directories('.'),
]

dependencies = [
  mathlib_dep,
  glib_dep,
  gtk_dep,
  gtk_wayland_dep,
  wayland_client_dep,
  wayland_protocols_dep,
]

c_args = [
  '-DLOCALEDIR="@0@"'.format(gnome_screenshot_localedir),
]

executable(
  'gshot',
  sources + resources,
  include_directories: include_directories,
  dependencies: dependencies,
  c_args: c_args,
  install: true
)
