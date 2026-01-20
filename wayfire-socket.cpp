
#include "wayfire-socket.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gio/gio.h>

#include <json.h>

#include <iostream>
#include <string>
#include <optional>
#include <deque>
#include <unordered_map>


static const char default_socket[] = "/tmp/wayfire-wayland-1.socket";

static GIOChannel *s_pIOChannel = NULL; // GIOChannel encapsulating the above socket
static unsigned int s_sidIO = 0; // source ID for the above added to the main loop

struct cb_data {
	uint32_t view_id; // view that this result belongs to or -1 if not interested (for other callbacks)
	const gchar *prop_name; // property that was requested
};
static std::deque<cb_data> s_cb_queue; // queue of pending get_view_property requests with cb_data elements

// all currently known views
static std::unordered_map<uint32_t, view_data> s_views;

static const char * const s_known_props[] = {
	"kde-appmenu-service-name",
	"kde-appmenu-object-path",
	"gtk-shell-app-menu-path",
	"gtk-shell-application-object-path",
	"gtk-shell-menubar-path",
	"gtk-shell-unique-bus-name",
	"gtk-shell-window-object-path",
	nullptr
};

static std::function<void(const view_data*)> focus_changed;


static void _handle_broken_socket (void)
{
	if (s_sidIO) g_source_remove (s_sidIO); // will close the socket and free s_pIOChannel as well
	s_sidIO = 0;
	s_pIOChannel = NULL;
	s_cb_queue.clear();
	s_views.clear();
	// maybe quit -- no use staying open if we cannot receive events anymore
}

/*
 * Read a message from socket and return its contents or NULL if there
 * is no open socket. The caller should free() the returned string when done.
 * The length of the message is stored in msg_len (if not NULL).
 * Note: this will block until there is a message to read.
 */
static char* _read_msg_full (uint32_t* out_len, GIOChannel *pIOChannel)
{
	const size_t header_size = 4;
	char header[header_size];
	if (g_io_channel_read_chars (pIOChannel, header, header_size, NULL, NULL) != G_IO_STATUS_NORMAL)
	{
		//!! TODO: should this be a failure or is it possible to recover?
		std::cerr << "Error reading from Wayfire's socket\n";
		return NULL;
	}
	
	uint32_t msg_len;
	memcpy(&msg_len, header, header_size);
	gsize n_read = 0;
	char* msg = (char*)g_malloc (msg_len + 1);
	if (msg_len)
	{
		if (g_io_channel_read_chars (pIOChannel, msg, msg_len, &n_read, NULL) != G_IO_STATUS_NORMAL
			|| n_read != msg_len)
		{
			//!! TODO: should we care if the whole message cannot be read at once?
			std::cerr << "Error reading from Wayfire's socket\n";
			g_free (msg);
			return NULL;
		}
	}
	msg[msg_len] = 0;
	if (out_len) *out_len = msg_len;
	return msg;
}
/* same as above, but use our static variable */
static char* _read_msg (uint32_t* out_len)
{
	if (! s_pIOChannel) return NULL;
	return _read_msg_full (out_len, s_pIOChannel);
}

/* Send a message to the socket. Return 0 on success, -1 on failure */
static int _send_msg_full (const char* msg, uint32_t len, GIOChannel *pIOChannel)
{
	const size_t header_size = 4;
	gsize n_written = 0;
	char header[header_size];
	memcpy(header, &len, header_size);
	if ((g_io_channel_write_chars (pIOChannel, header, header_size, &n_written, NULL) != G_IO_STATUS_NORMAL) ||
		(g_io_channel_write_chars (pIOChannel, msg, len, &n_written, NULL) != G_IO_STATUS_NORMAL) ||
		n_written != len)
	{
		std::cerr << "Error writing to Wayfire's socket\n";
		return -1;
	}
	return 0;
}

/* Call a Wayfire IPC method and try to check if it was successful. */
static void _call_ipc (struct json_object* data, cb_data cb = {(uint32_t)-1, nullptr}) {
	if (! s_pIOChannel)
	{
		// socket was already closed; we should not call _handle_broken_socket () again
		return;
	}
	
	size_t len;
	const char *tmp = json_object_to_json_string_length (data, JSON_C_TO_STRING_SPACED, &len);
	if (!(tmp && len))
	{
		std::cerr << "Cannot create JSON string\n";
		return;
	}
	
	if (_send_msg_full (tmp, len, s_pIOChannel) < 0)
	{
		_handle_broken_socket ();
		return;
	}
	
	// save the callback
	s_cb_queue.push_back(cb);
}



/* Get a property on a view */
static void _get_view_prop (uint32_t view_id, const gchar *prop_name)
{
	struct json_object *obj = json_object_new_object ();
	json_object_object_add (obj, "method", json_object_new_string ("window-rules/get-view-property"));
	struct json_object *data = json_object_new_object ();
	json_object_object_add (data, "id", json_object_new_uint64 (view_id));
	json_object_object_add (data, "property", json_object_new_string (prop_name));
	json_object_object_add (obj, "data", data);
	_call_ipc (obj, {view_id, prop_name});
	json_object_put (obj);
}




static uint32_t s_last_active = (uint32_t)-1; // last known active view (-1 means none)

static void _start_watch_events (void)
{
	const gchar *watch_req =
"{\"method\": \"window-rules/events/watch\", \"data\": {"
"\"events\": [\"view-focused\", \"view-mapped\", \"view-unmapped\"] } }";

	int ret = _send_msg_full (watch_req, strlen (watch_req), s_pIOChannel);
	if (ret < 0)
	{
		// socket error, abort
		_handle_broken_socket ();
		return;
	}
	
	s_cb_queue.push_back({(uint32_t)-1, NULL}); // dummy callback data
	
	// note: we do not call list-views, we will try to get the properties of views when we first see them
}

// Process an event on a view
static void _process_view (const gchar *event, const struct json_object *view)
{
	// check if this is the active view losing focus
	if (!view)
	{
		// in this case event != NULL
		if (!strcmp (event, "view-focused"))
		{
			// we don't really care, we can keep displaying the menu of the last active view
			// s_last_active = (uint32_t)-1;
		}
		else std::cerr << "Unexpected event with no view data:" << event << "\n";
		return;
	}
	
	// first, get the view's ID -- Wayfire uses 32-bit integers for now
	uint32_t id;
	errno = 0;
	struct json_object *tmp = json_object_object_get (view, "id");
	if (tmp) id = (uint32_t) json_object_get_uint64 (tmp);
	else errno = 1;
	if (errno)
	{
		std::cerr << "Cannot parse view ID\n";
		return; // TODO: should we cancel?
	}
	
	if (! strcmp (event, "view-unmapped"))
	{
		// remove from our hash table
		s_views.erase(id);
		if (id == s_last_active) focus_changed (nullptr);
	}
	else 
	{
		// first, check if this view is interesting to us
		tmp = json_object_object_get (view, "type");
		const char *str = tmp ? json_object_get_string (tmp) : NULL;
		if (!str)
		{
			std::cerr << "Cannot get type for view " << id << "\n";
			return;
		}
		if (!strcmp (str, "toplevel"))
		{
			
			if (! strcmp (event, "view-mapped"))
			{
				// this is a new view, try to get all of its properties
				for (const char * const *prop = s_known_props; *prop; ++prop)
					_get_view_prop (id, *prop);
			}
			else if (! strcmp (event, "view-focused"))
			{
				tmp = json_object_object_get (view, "title");
				str = tmp ? json_object_get_string (tmp) : NULL;
				if (!str) std::cerr << "Cannot get title for view " << id << "\n";
				
				auto it = s_views.find (id);
				if (it == s_views.end())
				{
					// we have not seen this view before, try to get its props
					for (const char * const *prop = s_known_props; *prop; ++prop)
						_get_view_prop (id, *prop);
					
					if (str) s_views[id].title = std::string(str);
				}
				else
				{
					// known view, signal that focus changed
					if (str) it->second.title = std::string(str);
					if (s_last_active != id) focus_changed (&it->second);
				}
				
				s_last_active = id;
			}
		}
	}
}


/***********************************************************************
 * parse incoming messages, init
 ***********************************************************************/

static gboolean _socket_cb (G_GNUC_UNUSED GIOChannel *pSource, GIOCondition cond, G_GNUC_UNUSED gpointer data)
{
	gboolean bContinue = FALSE;
	
	if ((cond & G_IO_HUP) || (cond & G_IO_ERR))
		std::cerr << "Wayfire socket disconnected\n";
	else if (cond & G_IO_IN)
	{
		// we should be able to read at least one complete message
		uint32_t len2 = 0;
		char* tmp2 = _read_msg (&len2);
		if (tmp2)
		{
			struct json_object *res = json_tokener_parse (tmp2);
			if (res)
			{
				bContinue = TRUE; // we could read a valid JSON, the connection is OK
				
				struct json_object *event = json_object_object_get (res, "event");
				if (event)
				{
					// try to process an event that we are subscribed to
					const char *event_str = json_object_get_string (event);
					if (!event_str) std::cerr << "Cannot parse event name!\n";
					else
					{
						// likely a view related event
						_process_view (event_str, json_object_object_get (res, "view"));
					}
				}
				else if (s_cb_queue.empty ())
				{
					std::cerr << "Missing callback in queue!\n";
				}
				else {
					const cb_data& head = s_cb_queue.front ();
					bool ok = false;
					
					struct json_object *result = json_object_object_get (res, "result");
					if (result)
					{
						const char *value = json_object_get_string (result);
						if (value && !strcmp(value, "ok")) ok = true;
					}
					
					if (!ok) std::cerr << "Error reply to request!\n";
					else if (head.view_id != (uint32_t)-1)
					{
						view_data& view_props = s_views[head.view_id]; // will add a new element if necessary
						
						struct json_object *tmp = json_object_object_get (res, "value");
						const char *value = tmp ? json_object_get_string (tmp) : nullptr;
						if (!value) std::cerr << "No value in result for property: " << head.prop_name << "\n";
						// add this property
						else if (! strcmp (head.prop_name, "kde-appmenu-service-name"))
							view_props.kde_service_name = std::string(value);
						else if (! strcmp (head.prop_name, "kde-appmenu-object-path"))
							view_props.kde_object_path = std::string(value);
						else if (! strcmp (head.prop_name, "gtk-shell-app-menu-path"))
							view_props.gtk_app_menu_path = std::string(value);
						else if (! strcmp (head.prop_name, "gtk-shell-application-object-path"))
							view_props.gtk_application_object_path = std::string(value);
						else if (! strcmp (head.prop_name, "gtk-shell-menubar-path"))
							view_props.gtk_menubar_path = std::string(value);
						else if (! strcmp (head.prop_name, "gtk-shell-unique-bus-name"))
							view_props.gtk_unique_bus_name = std::string(value);
						else if (! strcmp (head.prop_name, "gtk-shell-window-object-path"))
						{
							view_props.gtk_window_object_path = std::string(value);
							
							// this is the last property, signal if this is the current active view
							if (head.view_id == s_last_active) focus_changed (&view_props);
						}
					}
					
					s_cb_queue.pop_front ();
				}
				
				json_object_put (res);
			}
			else std::cerr << "Invalid JSON returned by Wayfire\n";
			free(tmp2);
		}
		// if we have no message, warning already shown, we will stop the connection
		// as bContinue == FALSE
	}
	
	if (!bContinue)
	{
		s_sidIO = 0;
		s_pIOChannel = NULL;
		s_cb_queue.clear();
		s_views.clear();
		return FALSE; // will also free our GIOChannel and close the socket -- TODO: quit the app as well !
	}
	
	return TRUE;
}

void init_wayfire_socket (std::function<void(const view_data*)>&& _focus_cb) {
	focus_changed = std::move(_focus_cb);
	
	const char* wf_socket = getenv("WAYFIRE_SOCKET");
	if (!wf_socket) wf_socket = default_socket;
	
	int wayfire_socket = socket (AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (wayfire_socket < 0) return;
	
	struct sockaddr_un sa;
	memset (&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy (sa.sun_path, wf_socket, sizeof (sa.sun_path) - 1);
	
	if (connect (wayfire_socket, (const struct sockaddr*)&sa, sizeof (sa))) {
		close (wayfire_socket);
		return;
	}
	
	GIOChannel *pIOChannel = g_io_channel_unix_new (wayfire_socket);
	if (!pIOChannel)
	{
		std::cerr << "Cannot create GIOChannel!\n";
		close (wayfire_socket);
		return;
	}
	
	g_io_channel_set_close_on_unref (pIOChannel, TRUE); // take ownership of wayfire_socket
	g_io_channel_set_encoding (pIOChannel, NULL, NULL); // otherwise GLib would try to validate received data as UTF-8
	g_io_channel_set_buffered (pIOChannel, FALSE); // we don't want GLib to block trying to read too much data
	
	s_sidIO = g_io_add_watch (pIOChannel, (GIOCondition) (G_IO_IN | G_IO_HUP | G_IO_ERR), _socket_cb, NULL);
	if (s_sidIO)
	{
		s_pIOChannel = pIOChannel;
		_start_watch_events ();
	}
	else std::cerr << "Cannot add socket IO event source!\n";
	
	g_io_channel_unref (pIOChannel); // note: ref taken by g_io_add_watch () if succesful
}

void fini_wayfire_socket ()
{
	_handle_broken_socket ();
}


