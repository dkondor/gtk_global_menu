
#include <string>
#include <optional>
#include <functional>

// all properties stored for a view
struct view_data {
	std::optional<std::string> title;
	std::optional<std::string> kde_service_name;
	std::optional<std::string> kde_object_path;
	std::optional<std::string> gtk_app_menu_path;
	std::optional<std::string> gtk_menubar_path;
	std::optional<std::string> gtk_window_object_path;
	std::optional<std::string> gtk_application_object_path;
	std::optional<std::string> gtk_unique_bus_name;
};

void init_wayfire_socket (std::function<void(const view_data*)>&& _focus_cb);
void fini_wayfire_socket ();

