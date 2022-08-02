#install_subdir(
#  'themes',
#  install_dir : join_paths(control_center_datadir, 'gnome-control-center')
#)

desktop = 'gooroom-themes.ini'

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
  install_dir: join_paths(control_center_datadir, 'gnome-control-center/themes')
)
