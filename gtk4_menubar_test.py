#!/usr/bin/env python3
#
#  gtk4_menubar_test.py -- simple example program to test GTK4 menubars
#  


import sys
import gi

gtk_version = '4.0'

if __name__ == '__main__':
	args = sys.argv[1:]
	for i in range(len(args)):
		if args[i] == '-g' or args[i] == '--gtk-version':
			v = int(args[i+1])
			if v == 3:
				gtk_version = '3.0'
			elif v != 4:
				raise BaseException(f'Unsupported GTK version: {v}')

gi.require_version('Gtk', gtk_version)
gi.require_version('GLib', '2.0')
from gi.repository import Gtk, GLib, Gio

def action_cb(action, par):
	name = action.get_name()
	print(f'Action activated: {name}')

def startup(app):
	menu_if = """
<interface>
	<menu id='menubar'>
		<submenu>
			<attribute name='label' translatable='yes'>_Something</attribute>
			<item>
				<attribute name='label' translatable='yes'>_Item 1</attribute>
				<attribute name='action'>win.item1</attribute>
			</item>
			<item>
				<attribute name='label' translatable='yes'>I_tem 2</attribute>
				<attribute name='action'>win.item2</attribute>
			</item>
		</submenu>
		<submenu>
			<attribute name='label' translatable='yes'>Sub_menu 2</attribute>
			<item>
				<attribute name='label' translatable='yes'>Some _other item</attribute>
				<attribute name='action'>win.item3</attribute>
			</item>
			<item>
				<attribute name='label' translatable='yes'>One _more item</attribute>
				<attribute name='action'>win.item4</attribute>
			</item>
		</submenu>
	</menu>
</interface>
	"""
	builder = Gtk.Builder.new_from_string(menu_if, -1)
	menubar = builder.get_object("menubar")
	app.set_menubar(menubar)

def activate(app):
	win = Gtk.ApplicationWindow.new(app)
	win.set_show_menubar(True)
	
	for i in range(1, 5):
		a = Gio.SimpleAction.new(f'item{i}', None)
		a.connect("activate", action_cb)
		win.add_action(a)
	
	win.set_title("Test window")
	win.set_default_size(300, 200)
	win.present()


def main(args):
    app = Gtk.Application.new("org.example.menubar", Gio.ApplicationFlags.DEFAULT_FLAGS)
    app.connect("startup", startup)
    app.connect("activate", activate)
    
    return app.run(None)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

