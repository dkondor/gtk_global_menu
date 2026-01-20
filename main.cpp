/*
 * main.cpp -- test program for global menus
 * 
 * Copyright 2025 Daniel Kondor <kondor.dani@gmail.com>
 * 
 * This is free and unencumbered software released into the public domain.
 * 
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 * 
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */



#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <glibmm.h>

#include "dbusmenu.hpp"
#include "wayfire-socket.hpp"


// main window
static GtkWidget *window = nullptr;
static GtkPopoverMenuBar *menubar = nullptr;
static GtkLabel *label = nullptr;

static GDBusConnection *bus = nullptr;
static DbusMenuModel *kde_model = nullptr;

static void focus_changed (const view_data* view)
{
	if (! (label && menubar)) return;
	if (view && view->title) gtk_label_set_label (label, view->title->c_str ());
	else gtk_label_set_label (label, "(unknown)");
	
	gtk_popover_menu_bar_set_menu_model (menubar, nullptr);
	gtk_widget_insert_action_group (GTK_WIDGET (menubar), "app", NULL);
	gtk_widget_insert_action_group (GTK_WIDGET (menubar), "win", NULL);
	gtk_widget_insert_action_group (GTK_WIDGET (menubar), "unity", NULL);
	gtk_widget_insert_action_group (GTK_WIDGET (menubar), "tmp_action", NULL);
	
	if (kde_model)
	{
		delete kde_model;
		kde_model = nullptr;
	}
	
	if (!view) return;
	
	// GTK case
	if (view->gtk_unique_bus_name && view->gtk_menubar_path)
	{
		GDBusMenuModel *model = g_dbus_menu_model_get (bus, view->gtk_unique_bus_name->c_str (), view->gtk_menubar_path->c_str ());
		
		if (model) {
			gtk_popover_menu_bar_set_menu_model(menubar, G_MENU_MODEL (model));
			g_object_unref (model);
			
			if (view->gtk_application_object_path) {
				GDBusActionGroup *grp = g_dbus_action_group_get (bus,
					view->gtk_unique_bus_name->c_str (), view->gtk_application_object_path->c_str ());
				if (grp) {
					gtk_widget_insert_action_group (GTK_WIDGET (menubar), "app", G_ACTION_GROUP (grp));
					g_object_unref (grp);
				}
				else fprintf (stderr, "Error retrieving app action group!\n");
			}
			if (view->gtk_window_object_path) {
				GDBusActionGroup *grp = g_dbus_action_group_get (bus,
					view->gtk_unique_bus_name->c_str (), view->gtk_window_object_path->c_str ());
				if (grp) {
					gtk_widget_insert_action_group (GTK_WIDGET (menubar), "win", G_ACTION_GROUP (grp));
					g_object_unref (grp);
				}
				else fprintf (stderr, "Error retrieving window action group!\n");
			}
			
			// fallback
			GDBusActionGroup *grp = g_dbus_action_group_get (bus,
				view->gtk_unique_bus_name->c_str (), view->gtk_menubar_path->c_str ());
			if (grp)
			{
				gtk_widget_insert_action_group (GTK_WIDGET (menubar), "unity", G_ACTION_GROUP (grp));
				g_object_unref (grp);
			}
		}
		else fprintf(stderr, "Error retrieving menu model!\n");
	}
	else if (view->kde_service_name && view->kde_object_path) {
		// KDE / com.canonical.dbusmenu case
		kde_model = new DbusMenuModel ();
		kde_model->connect (view->kde_service_name->c_str (), view->kde_object_path->c_str (), "tmpaction");
		kde_model->signal_action_group().connect([=] ()
		{
			auto model = kde_model->get_menu();
			gtk_popover_menu_bar_set_menu_model (menubar, G_MENU_MODEL (model->gobj ()));
			auto grp = kde_model->get_action_group();
			gtk_widget_insert_action_group (GTK_WIDGET (menubar), "tmpaction", G_ACTION_GROUP (grp->gobj ()));
		});
	}
}

// #define SELF_NAME "gtk_global_menu_test"

int main() {
	Glib::init();
	gtk_init();
	
	bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
	if (!bus)
	{
		fprintf(stderr, "Cannot connect to DBus!\n");
		return 1;
	}
	
	init_wayfire_socket (focus_changed);
	// g_set_prgname (SELF_NAME);
	
	window = gtk_window_new();
	
	gtk_layer_init_for_window (GTK_WINDOW (window));
	gtk_layer_set_namespace (GTK_WINDOW (window), "global-menu-test");
	gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
	gtk_layer_set_layer (GTK_WINDOW (window), GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_keyboard_mode (GTK_WINDOW (window), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);
	
	gtk_window_set_title (GTK_WINDOW (window), "Gtk global menu test");
	
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	GtkWidget *lbl  = gtk_label_new("(no active app)");
	GtkWidget *menu = gtk_popover_menu_bar_new_from_model (NULL);
	gtk_box_append (GTK_BOX (hbox), lbl);
	gtk_box_append (GTK_BOX (hbox), menu);
	gtk_window_set_child (GTK_WINDOW (window), hbox);
	gtk_widget_set_visible (window, TRUE);
	gtk_window_present (GTK_WINDOW (window));
	
	label = GTK_LABEL (lbl);
	menubar = GTK_POPOVER_MENU_BAR(menu);
	
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK (gtk_window_destroy), NULL);
	
	while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
		g_main_context_iteration (nullptr, TRUE);

	fini_wayfire_socket ();
	return 0;
}

