project(
	'gtk_global_menu',
	'cpp',
	version: '0.0.1',
	license: 'MIT',
	meson_version: '>=0.50.0',
	default_options: [
		'warning_level=2',
		'werror=false',
	],
)


gtk = dependency('gtk4')
layershell = dependency('gtk4-layer-shell-0')
glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
giounix = dependency('gio-unix-2.0')
giomm = dependency('giomm-2.68')
dbusmenu = dependency('dbusmenu-glib-0.4')
jsonc = dependency('json-c')

sources = ['dbusmenu.cpp', 'dbusmenu.hpp', 'wayfire-socket.cpp', 'wayfire-socket.hpp', 'main.cpp']

menu1 = executable('menu1', sources, dependencies: [gtk, layershell, gio, giounix, giomm, glib, dbusmenu, jsonc])

