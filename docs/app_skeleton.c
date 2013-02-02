// This file includes everything you need to write an app for Quick.IO
#include "qio_app.h"

/**
 * The structure representing the events in the queue.
 */
typedef struct {
	/**
	 * The number of babies born in this event.
	 */
	guint64 babies_born;
	
	/**
	 * A reference to the client, for the callback
	 */
	client_t *client;
	
	/**
	 * For if we want to send a callback
	 */
	callback_t client_callback;
} pending_event_t;

/**
 * The location where the configuration options are stored.
 *
 * Set these to their default values before running.
 */
static gint _age = 23;
static guint64 _earth_population = 7000000000;
static gchar *_planet_name = NULL;
static gint _friends_count = 0;
static gchar **_friends = NULL;

/**
 * All of the configuration options for this application.
 *
 * The following just demonstrates how to use the different option types.
 *
 * Valid types:
 *   - e_int
 *   - e_string
 *   - e_string_array
 *   - e_uint64
 */
static config_file_entry_t _config_options[] = {
	{"age", e_int, &_age},
	{"earth-population", e_uint64, &_earth_population},
	{"friends", e_string_array, &_friends, &_friends_count},
	{"planet-name", e_str, &_planet_name},
};

/**
 * All of the events that we plan on registering.
 */
static event_handler_t *_baby_born;
static event_handler_t *_population;

/**
 * Holds the pending events
 */
static GAsyncQueue *_events;

/**
 * Handles babies being born.
 */
static status_t _baby_born(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	pending_event_t *e = g_slice_alloc0(sizeof(*e));
	
	// You should probably do error checking here
	e->babies_born = g_ascii_strtoull(event->data);
	
	// Client is reference counted, so be sure to increment him
	e->client = client;
	client_ref(client);
	
	// This is guaranteed to be right, no error checking needed
	e->client_callback = event->client_callback;
	
	// Put the birth into the async queue for processing in the app's thread
	// This is just a good way to synchronize threads
	g_async_queue_push(_events, e);
	
	// Indicate that we are going async with this event
	return CLIENT_ASYNC;
}

/**
 * The `on` parameter here is the equivalent of evs_server_on
 */
gboolean app_init(app_on on) {
	GError *error = NULL;
	if (!option_parse_config_file(_app_name, _config_options, G_N_ELEMENTS(_config_options), &error)) {
		ERRORF("Could not load configuration for app \"ihr\": %s", error->message);
		return FALSE;
	}
	
	_events = g_async_queue_new();
	
	// Don't let anyone subscribe to this event
	_baby_born = on("/baby/born", _baby_born, evs_no_subscribe, NULL, FALSE);
	
	// People subscribe to the updated population
	_population = on("/population", NULL, NULL, NULL, FALSE);
	
	return TRUE;
}

/**
 * Time to run the application loop, just waiting for events.
 */
gboolean app_run(app_on on) {
	while (TRUE) {
		pending_event_t *e = g_async_queue_pop(_events);
		
		_earth_population += e->babies_born;
		
		// Send a blank callback to the client, indicating the event is processed
		evs_client_send_callback(e->client, e->client_callback, 0, d_plain, "");
		
		// Send out a message to everyone subscribed to /population with the updated
		// population of the earth
		gchar buff[16];
		snprintf(buff, sizeof(buff), "%" G_GUINT64_FORMAT, _earth_population);
		evs_client_pub_event(_population, NULL, d_plain, _earth_population);
		
		// Done with our reference to the client, if we don't do this, that's a memory leak
		client_unref(e->client);
		
		g_slice_free1(sizeof(*e), e);
	}
}
