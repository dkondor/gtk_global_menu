#!/usr/bin/python3

# A simple client for displaying global menus of compatible apps.

import os, sys
import socket
import json
from collections import deque

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
gi.require_version('GLib', '2.0')
gi.require_version('DbusmenuGtk3', '0.4')

from gi.repository import Gtk, Gdk, GLib, Gio, DbusmenuGtk3


class GlobalMenuClient:
	
	def __init__(self):
		self.queue = deque()
		self.conn = Gio.bus_get_sync(Gio.BusType.SESSION, None)
		
		wf_socket = os.getenv("WAYFIRE_SOCKET")
		if wf_socket is None:
			raise BaseException('No socket name set!')
		
		self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		self.sock.connect(wf_socket)
		self.ch = GLib.IOChannel.unix_new(self.sock.fileno())
		self.ch.set_encoding(None)
		self.ch.set_buffered(False)
		
		GLib.io_add_watch(self.ch, GLib.PRIORITY_HIGH, GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, self.sock_event)
		
		self.send_msg({"method": "window-rules/events/watch", "data":
			{"events": ["view-focused", "view-mapped", "view-unmapped"]}})
		self.queue.append(None)
		
		self.active_view_id = None
		self.entry = None
		self.views = {}
		self.self_id = None
		self.menubtn = None
		self.last_menu = None

		self.win1 = Gtk.Window()
		self.title = 'App menu test'
		self.win1.set_title(self.title)
		self.win1.set_size_request(400, 300)

		box1 = Gtk.Box(orientation = Gtk.Orientation.VERTICAL)
		box2 = Gtk.Box(orientation = Gtk.Orientation.HORIZONTAL)
		lbl1 = Gtk.Label.new('Active app:  ')
		self.entry = Gtk.Entry()
		box2.add(lbl1)
		box2.add(self.entry)
		box1.add(box2)

		btn = Gtk.MenuButton.new()
		btn.set_label('Show menu')
		self.menubtn = btn
		btn.connect("clicked", self.show_menu_cb)
		box1.add(btn)
		
		self.win1.add(box1)
		self.win1.connect('destroy', Gtk.main_quit)
		self.win1.show_all()

	def setup_menu(self, props):
		self.menubtn.insert_action_group("app", None)
		self.menubtn.insert_action_group("win", None)
		self.menubtn.insert_action_group("unity", None)
		self.menubtn.set_menu_model(None)
		self.menubtn.set_popup(None)
		self.last_menu = None
		
		if props is None:
			return
		
		if "gtk-shell-unique-bus-name" in props and "gtk-shell-menubar-path" in props:
			name = props["gtk-shell-unique-bus-name"]
			path = props["gtk-shell-menubar-path"]
			win_path = props["gtk-shell-window-object-path"] if "gtk-shell-window-object-path" in props else None
			app_path = props["gtk-shell-application-object-path"] if "gtk-shell-application-object-path" in props else None
			
			menumodel = Gio.DBusMenuModel.get(self.conn, name, path)
			self.menubtn.set_menu_model(menumodel)
			if app_path is not None:
				app_actions = Gio.DBusActionGroup.get(self.conn, name, app_path)
				self.menubtn.insert_action_group("app", app_actions)
			if win_path is not None:
				win_actions = Gio.DBusActionGroup.get(self.conn, name, win_path)
				self.menubtn.insert_action_group("win", win_actions)
			# fallback
			unity_actions = Gio.DBusActionGroup.get(self.conn, name, path)
			self.menubtn.insert_action_group("unity", unity_actions)
		elif "kde-appmenu-service-name" in props and "kde-appmenu-object-path" in props:
			name = props["kde-appmenu-service-name"]
			path = props["kde-appmenu-object-path"]
			if name is not None and path is not None:
				menu = DbusmenuGtk3.Menu.new(name, path)
				if menu:
					menu.show_all()
					self.menubtn.set_popup(menu)
					self.last_menu = menu

	def read_msg(self):
		tmp1 = self.ch.read(4) # header
		l2 = int.from_bytes(tmp1, byteorder = 'little')
		tmp2 = self.ch.read(l2) # message
		return json.loads(tmp2)
	
	def send_msg(self, msg):
		data = json.dumps(msg).encode("utf8")
		l1 = len(data)
		len_bytes = l1.to_bytes(4, byteorder="little")
		self.ch.write_chars(len_bytes, 4)
		self.ch.write_chars(data, l1)
	
	def get_view_props(self, id1):
		prop_names = ["kde-appmenu-service-name", "kde-appmenu-object-path",
			"gtk-shell-app-menu-path", "gtk-shell-application-object-path",
			"gtk-shell-menubar-path", "gtk-shell-unique-bus-name",
			"gtk-shell-window-object-path"]
		for p in prop_names:
			self.send_msg({"method": "window-rules/get-view-property",
				"data": {"id": id1, "property": p}})
			self.queue.append((id1, p))

	def sock_event(self, ch, cond):
		msg = self.read_msg()
		if msg is None:
			return False # should not happen, will likely throw an exception instead
		if "event" in msg:
			if msg["event"] == "view-focused":
				if ("view" in msg) and (msg["view"] is not None):
					id1 = msg["view"]["id"]
					if self.self_id and (id1 != self.self_id) and msg["view"]["type"] == "toplevel" and (id1 != self.active_view_id):
						self.entry.set_text(msg["view"]["title"])
						if id1 in self.views:
							self.setup_menu(self.views[id1])
						else:
							self.setup_menu(None)
							self.get_view_props(id1)
						self.active_view_id = id1
			elif msg["event"] == "view-mapped":
				if ("view" in msg) and (msg["view"] is not None):
					id1 = msg["view"]["id"]
					if msg["view"]["title"] == self.title:
						self.self_id = id1
					if msg["view"]["type"] == "toplevel":
						self.get_view_props(id1)
			elif msg["event"] == "view-unmapped":
				if ("view" in msg) and (msg["view"] is not None):
					id1 = msg["view"]["id"]
					if id1 in self.views:
						del self.views[id1]
					if id1 == self.active_view_id:
						self.setup_menu(None)
						self.active_view_id = None
		else:
			# this should be a reply to one of our queries
			x = self.queue.popleft()
			if x is not None:
				view_id = x[0]
				prop_name = x[1]
				if not view_id in self.views:
					self.views[view_id] = {}
				if "value" in msg:
					self.views[view_id][prop_name] = msg["value"]
				if prop_name == "gtk-shell-window-object-path":
					# last one, re-check
					if view_id == self.active_view_id:
						self.setup_menu(self.views[view_id])
		return True

	def show_menu_cb(self, btn):
		if self.last_menu:
			self.last_menu.popup_at_widget(btn, Gdk.Gravity.SOUTH_WEST, Gdk.Gravity.NORTH_WEST, None)


def main(args):
	Gtk.init()
	cl = GlobalMenuClient()
	Gtk.main()
	return 0

if __name__ == '__main__':
	sys.exit(main(sys.argv[1:]))




