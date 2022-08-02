panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

sources = files(
  'cc-themes-theme.c',
  'cc-themes-panel.c',
)

resource_data = files(
  'themes.ui',
  'themes-theme.ui',
  'check-icon.svg',
  'style.css',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  cups_dep,
  m_dep,
  polkit_gobject_dep,
  dependency('smbclient')
]

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

themes_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc, shell_inc],
  dependencies: deps,
  c_args: cflags
)
panels_libs += [ themes_panel_lib ]

subdir('data')
