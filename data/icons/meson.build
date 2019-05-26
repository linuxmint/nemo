publicIcons = [
  'hicolor/apps/16x16/nemo.png',
  'hicolor/apps/22x22/nemo.png',
  'hicolor/apps/24x24/nemo.png',
  'hicolor/apps/32x32/nemo.png',
  'hicolor/apps/scalable/nemo.svg',
  'hicolor/actions/16x16/nemo-eject.png',
  'hicolor/actions/16x16/menu-bullet.png',
  'hicolor/actions/16x16/menu-sort-up.png',
  'hicolor/actions/16x16/menu-sort-up-free.png',
  'hicolor/actions/16x16/menu-sort-down.png',
  'hicolor/actions/16x16/menu-sort-down-free.png',
  'hicolor/actions/16x16/menu-sort-right.png',
  'hicolor/actions/16x16/menu-sort-right-free.png',
  'hicolor/actions/16x16/menu-sort-left.png',
  'hicolor/actions/16x16/menu-sort-left-free.png',
  'hicolor/actions/16x16/menu-none.png',
  'hicolor/actions/32x32/nemo-eject.png',
  'hicolor/actions/scalable/view-compact-symbolic.svg',
  'hicolor/actions/scalable/location-symbolic.svg',
  'hicolor/actions/scalable/mount-archive-symbolic.svg',
  'hicolor/actions/scalable/sidebar-hide-symbolic.svg',
  'hicolor/actions/scalable/sidebar-show-symbolic.svg',
  'hicolor/actions/scalable/sidebar-tree-symbolic.svg',
  'hicolor/actions/scalable/sidebar-places-symbolic.svg',
  'hicolor/actions/scalable/nemo-auto-arrange-symbolic.svg',
  'hicolor/actions/scalable/nemo-desktop-scale-symbolic.svg',
  'hicolor/actions/scalable/nemo-horizontal-layout-symbolic.svg',
  'hicolor/actions/scalable/nemo-horizontal-layout-wide-symbolic.svg',
  'hicolor/actions/scalable/nemo-vertical-layout-symbolic.svg',
  'hicolor/actions/scalable/nemo-vertical-layout-wide-symbolic.svg',
  'hicolor/devices/scalable/drive-removable-media-usb-symbolic.svg',
  'hicolor/status/48x48/progress-0.png',
  'hicolor/status/48x48/progress-10.png',
  'hicolor/status/48x48/progress-20.png',
  'hicolor/status/48x48/progress-30.png',
  'hicolor/status/48x48/progress-40.png',
  'hicolor/status/48x48/progress-50.png',
  'hicolor/status/48x48/progress-60.png',
  'hicolor/status/48x48/progress-70.png',
  'hicolor/status/48x48/progress-80.png',
  'hicolor/status/48x48/progress-90.png',
  'hicolor/status/48x48/progress-100.png',
  'hicolor/status/scalable/nemo-bookmark-not-found-symbolic.svg',
]
privateIcons = [
  'hicolor/emblems/16x16/emblem-note.png',
  'hicolor/emblems/24x24/emblem-note.png',
  'hicolor/emblems/48x48/emblem-note.png',
  'hicolor/emblems/scalable/emblem-pinned.svg'
]
noInstallIcons = [
  'hicolor/apps/16x16/nemo.svg',
  'hicolor/apps/22x22/nemo.svg',
  'hicolor/apps/32x32/nemo.svg',
]

foreach icon : publicIcons
  nameParts = icon.split('/')
  theme   = nameParts[0]
  context = nameParts[1]
  size    = nameParts[2]
  file    = nameParts[3]
  install_data(icon,
    install_dir: join_paths(get_option('datadir'), 'icons', theme, size, context)
  )
endforeach


foreach icon : privateIcons
  nameParts = icon.split('/')
  theme   = nameParts[0]
  context = nameParts[1]
  size    = nameParts[2]
  file    = nameParts[3]
  install_data(icon,
    install_dir: join_paths(get_option('datadir'), 'nemo', 'icons', theme, size, context)
  )
endforeach

