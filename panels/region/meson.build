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

polkit_conf = configuration_data()
polkit_conf.set('libexecdir', control_center_libexecdir)

polkit = 'org.gnome.controlcenter.region.policy'

polkit_in = configure_file(
  input: polkit + '.in.in',
  output: polkit + '.in',
  configuration: polkit_conf
)

i18n.merge_file(
  polkit,
  input: polkit_in,
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

sources = files(
  'cc-region-panel.c',
  'cc-format-chooser.c',
)

resource_data = files(
  'cc-format-chooser.ui',
  'cc-region-panel.ui',
  'view-layout-symbolic.svg',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  accounts_dep,
  gnome_desktop_dep,
  liblanguage_dep,
  polkit_gobject_dep
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)
