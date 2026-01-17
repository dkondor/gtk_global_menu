# GTK Global menu test

Test application for global menus on Wayfire. Requires functionality in this branch: https://github.com/dkondor/wayfire/tree/global_menu_ipc

### Running

First, start the "registrar" program:
```
python3 appmenu-registrar.py
```

Then the main script:
```
python3 gtk_global_menu.py
```

The app-id of the last active app is displayed, and if it supports global menus, its menu can be shown by clicking on the "Show menu" button. Note: in some case, the active app is not correctly detected and you might need to switch away and back to it for things to work.

### Making apps work

So far, I've tested it with GTK3 and Qt5 apps, specifically with Gedit, Inkscape and Kate (versions available in Ubuntu 24.04). The following are required to make apps actually export their menus:

1. A menu "registrar" needs to be running -- this is a program that implements the `com.canonical.AppMenu.Registrar` DBus interface. This actually plays no role on Wayland, but seems to be required to be present to "trigger" exporting menus. This is provided here as `appmenu-registrar.py`.

2. For GTK3 apps, you need to set the `GTK_MODULES` environment variable to include `appmenu-gtk-module`

3. For Qt apps, you need to set the `XDG_CURRENT_DESKTOP` environment variable to start with `KDE`


## License

The "registrar" program (`appmenu-registrar.py`) was originally developed as part of Cairo-Dock and is available under GPL3. The main appmenu script is based on the [pywayfire](https://github.com/WayfireWM/pywayfire) package that is part of Wayfire, and is available under the MIT license.



