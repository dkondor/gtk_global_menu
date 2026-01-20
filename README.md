# GTK Global menu test

Test applications for global menus on Wayfire. Requires functionality in this branch: https://github.com/dkondor/wayfire/tree/global_menu_ipc

This repository contains two versions of a global menu client:
 - A Python version using GTK3 and displaying menus attached to a button in a normal window
 - A C++ version using GTK4 and displaying a menubar at the top of the screen in a layer-shell window

Both are bare-bones and are meant for demonstration purposes only.

## Requirements

Wayfire:
 - This Wayfire branch: https://github.com/dkondor/wayfire/tree/global_menu_ipc
 - You need to enable the following plugins: `gtk-shell ipc-rules ipc window-rules kde-appmenu`

For the Python client:
 - GObject introspection support (e.g. `python3-gi` package on Debian / Ubuntu)
 - GIR libraries GTK3, GLib, Gio, gtk-layer-shell (e.g. `gir1.2-gtk-3.0` and `gir1.2-gtklayershell-0.1` packages on Debian / Ubuntu)

For the C++ client:
 - meson (version 0.50 or later)
 - Development libraries for GTK4, GLib, Gio (e.g. `libgtk-4-dev` package and its dependencies on Debian / Ubuntu)
 - Development libraries for glibmm, giomm (e.g. `libglibmm-2.68-dev` on Debian / Ubuntu)
 - Development library for [gtk4-layer-shell](https://github.com/wmww/gtk4-layer-shell)
 - Development library for [json-c](https://github.com/json-c/json-c/) (e.g. `libjson-c-dev` on Debian / Ubuntu)

### Compiling the C++ client

You can compile it the standard way using meson:
```
meson setup -Dbuildtype=debug build
ninja -C build
```

The result will be a `build/menu1` executable


## Running

First, start the "registrar" program:
```
python3 appmenu-registrar.py
```

### Python client

Start the the main script:
```
python3 gtk_global_menu.py
```

The app-id of the current active app is displayed, and if it supports global menus, its menu can be shown by clicking on the "Show menu" button. Note: in some case, the active app is not correctly detected and you might need to switch away and back to it for things to work.

### C++ client

Start the executable:
```
build/menu1
```

It will show up at the top edge of the screen, displaying the app-id of the current active app, and if it supports global menus, its menubar will also be shown. Note: you may need to switch away and back after starting.

## Making apps work

So far, I've tested it with GTK3 and Qt5 apps, specifically with Gedit, Inkscape and Kate (versions available in Ubuntu 24.04). The following are required to make apps actually export their menus:

1. A menu "registrar" needs to be running -- this is a program that implements the `com.canonical.AppMenu.Registrar` DBus interface. This actually plays no role on Wayland, but seems to be required to be present to "trigger" exporting menus. This is provided here as `appmenu-registrar.py`.

2. For GTK3 apps, you need to set the `GTK_MODULES` environment variable to include `appmenu-gtk-module`

3. For Qt apps, you need to set the `XDG_CURRENT_DESKTOP` environment variable to start with `KDE`

Note: as far as I can tell, GTK4 apps will not work.

## License

The "registrar" program (`appmenu-registrar.py`) was originally developed as part of Cairo-Dock and is available under GPL3. The `dbusmenu.hpp` and `dbusmenu.cpp` files are part of [wf-shell](https://github.com/WayfireWM/wf-shell/) and are available under the MIT license. The rest of the code in this repository is released into the public domain. See also the [copyright](copyright) file.



