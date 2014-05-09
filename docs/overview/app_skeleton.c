// This file includes everything you need to write an app for QuickIO
#include "quickio.h"

/**
 * All events in this application are prefixed to /skeleton
 */
#define EV_PREFIX "/skeleton"

/**
 * The structure representing the events in the queue.
 */
struct pending {
	/**
	 * The number of babies born in this event.
	 */
	gint64 babies_born;

	/**
	 * A reference to the client, for the callback
	 */
	struct client *client;

	/**
	 * For if we want to send a callback
	 */
	evs_cb_t client_cb;
};

/**
 * The thread for running all async tasks; will be joined on exit.
 */
static GThread *_thread;

/**
 * Where all events are sent for processing; in this application, it's not
 * really necessary as we're just processing integer increments, but it's
 * a good way to show how to do things asynchronously.
 */
static GAsyncQueue *_events;

/**
 * The handler for population events
 */
struct event *_ev_population;

/**
 * The population of the earth, as is known right now. This value is only
 * ever updated inside the skeleton thread.
 */
static gint64 _population;

/**
 * If you need external configuration for an app, it's already provided for you.
 * Check out quick-event's config documentation for all options.
 */
static struct qev_cfg _cfg[] = {
	{	.name = "population",
		.description = "How many people there are on earth",
		.type = QEV_CFG_INT64,
		.val.i64 = &_population,
		.defval.i64 = 7000000000,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
};

static enum evs_status _ev_baby_born_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	struct pending *p;
	gint64 babies_born;
	enum qev_json_status status;

	// This will modify the contents of `json`
	status = qev_json_unpack(json, NULL, "%d", &babies_born);
	if (status != QEV_JSON_OK) {
		evs_err_cb(client, client_cb, CODE_BAD, "could not parse JSON", NULL);
		goto out;
	}

	if (babies_born <= 0) {
		// Should probably give a better error message here
		evs_err_cb(client, client_cb, CODE_BAD, "invalid number of babies born", NULL);
		goto out;
	}

	p = g_slice_alloc(sizeof(*p));
	p->babies_born = babies_born;
	p->client = qev_ref(client);
	p->client_cb = client_cb;

	g_async_queue_push(_events, p);

out:
	return EVS_STATUS_HANDLED;
}

static void* _run(void *nothing G_GNUC_UNUSED)
{
	GString *buff = qev_buffer_get();

	while (TRUE) {
		struct pending *p = g_async_queue_pop(_events);

		// Once we get a NULL, assume we're exiting. The server finishes all
		// events before calling exit, so no more events will ever be in the
		// queue.
		if (p == NULL) {
			break;
		}

		_population += p->babies_born;

		// Send the client a callback asynchronously.
		evs_cb(p->client, p->client_cb, NULL);
		qev_unref(p->client);
		g_slice_free1(sizeof(*p), p);

		// Broadcast the new world population to everyone
		qev_buffer_clear(buff);
		qev_json_pack(buff, "%d", _population);
		evs_broadcast(_ev_population, NULL, buff->str);
	}

	qev_buffer_put(buff);

	return NULL;
}

static gboolean _app_init()
{
	// Read any configuration values set straight into our variables
	qev_cfg_add("skeleton", _cfg, G_N_ELEMENTS(_cfg));

	_events = g_async_queue_new();
	_thread = g_thread_new("skeleton", _run, NULL);

	// Will be automatically configured with the rest of quick-event's
	// configuration loading
	qev_cfg_add("skeleton", _cfg, G_N_ELEMENTS(_cfg));

	// No one may subscribe to this event
	evs_add_handler(EV_PREFIX, "/baby-born",
		_ev_baby_born_handler, evs_no_on, NULL, FALSE);

	// Anyone may subscribe to this event
	_ev_population = evs_add_handler(EV_PREFIX, "/population",
		NULL, NULL, NULL, FALSE);

	return TRUE;
}

static gboolean _app_exit()
{
	g_async_queue_push(_events, NULL);
	g_thread_join(_thread);
	_thread = NULL;

	g_async_queue_unref(_events);
	_events = NULL;

	return TRUE;
}

static gboolean _app_test()
{
	// Used to run test cases on the application
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	_app_exit);

QUICKIO_APP_TEST(_app_test);
