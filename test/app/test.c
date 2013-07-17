/**
 * THIS IS REALLY DIRTY AND I CAN'T BELIEVE I'M DOING THIS, but it's cleaner to do it this way
 * than to attempt to modify qio_app.h to work with test stuffs that are ONLY EVER needed
 * in this single application, so just include all QIO stuffs (included with utils_stats.h),
 * and cry yourself to sleep.
 */

#include "test/utils_stats.h"

static guint _app_id = 0;
static gchar *_app_name = NULL;
static gchar *_app_dir = NULL;
void qio_set_app_opts(const guint app_id, const gchar *name, const gchar *dir) {
	_app_id = app_id;
	_app_name = g_strdup(name);
	_app_dir = g_strdup(dir);
}

static GAsyncQueue *_work;

struct work {
	struct client *client;
	struct event_handler *handler;
	GPtrArray *extra;
	callback_t client_callback;
	gboolean success;
};

static enum status _test_handler(
	struct client *client,
	struct event_handler *handler,
	struct event *event,
	GString *response)
{
	TEST_STATS_INC(apps_client_handler);
	return CLIENT_GOOD;
}

static enum status _test_handler_on(
	struct client *client,
	const struct event_handler *handler,
	GPtrArray *extra,
	const callback_t client_callback)
{
	TEST_STATS_INC(apps_client_handler_on);

	return CLIENT_GOOD;
}

static enum status _test_handler_on_reject(
	struct client *client,
	const struct event_handler *handler,
	GPtrArray *extra,
	const callback_t client_callback)
{
	return CLIENT_ERROR;
}

static enum status _test_handler_on_async(
	struct client *client,
	const struct event_handler *handler,
	GPtrArray *extra,
	const callback_t client_callback)
{
	struct work *work = g_slice_alloc0(sizeof(*work));
	work->client = client;
	work->handler = (struct event_handler*)handler;
	work->extra = extra;
	work->client_callback = client_callback;
	work->success = TRUE;

	client_ref(client);
	g_ptr_array_ref(extra);

	g_async_queue_push(_work, work);

	return CLIENT_ASYNC;
}

static enum status _test_handler_on_async_reject(
	struct client *client,
	const struct event_handler *handler,
	GPtrArray *extra,
	const callback_t client_callback)
{
	struct work *work = g_slice_alloc0(sizeof(*work));
	work->client = client;
	work->handler = (struct event_handler*)handler;
	work->extra = extra;
	work->client_callback = client_callback;
	work->success = FALSE;

	client_ref(client);
	g_ptr_array_ref(extra);

	g_async_queue_push(_work, work);

	return CLIENT_ASYNC;
}

static void _test_handler_off(
	struct client *client,
	const struct event_handler *handler,
	GPtrArray *extra)
{
	TEST_STATS_INC(apps_client_handler_off);
}

gboolean app_init(app_on on)
{
	_work = g_async_queue_new();

	on("/event", _test_handler, _test_handler_on, _test_handler_off, TRUE);
	on("/children/reject", NULL, _test_handler_on_reject, NULL, TRUE);
	on("/async", NULL, _test_handler_on_async, NULL, TRUE);
	on("/async/reject", NULL, _test_handler_on_async_reject, NULL, TRUE);
	return TRUE;
}

gboolean app_run()
{
	TEST_STATS_INC(apps_run);

	while (TRUE) {
		struct work *work = g_async_queue_pop(_work);

		evs_client_app_sub_cb(work->client, work->handler,
						work->extra, work->client_callback,
						work->success);
		client_unref(work->client);
		g_ptr_array_unref(work->extra);

		g_slice_free1(sizeof(*work), work);
	}

	return TRUE;
}

void app_client_connect(struct client *client)
{
	TEST_STATS_INC(apps_client_connect);

	client_set(client, "test", "test");
}

void app_client_close(struct client *client)
{
	TEST_STATS_INC(apps_client_close);

	// Access data on the client to ensure it's accessible to callbacks
	client_has(client, "test");
	client_get(client, "test");
}

void app_evs_client_subscribe(
	struct client *client,
	const gchar *event_path,
	GPtrArray *extra,
	const guint extra_len)
{
	TEST_STATS_INC(apps_client_subscribe);
}

void app_evs_client_unsubscribe(
	struct client *client,
	const gchar *event_path,
	GPtrArray *extra,
	const guint extra_len)
{
	TEST_STATS_INC(apps_client_unsubscribe);
}

void app_stats_flush(apps_stats_cb stat)
{
	stat("test.key", 2);
	stat("test.some.craziness0", 2567234);
	stat("test.some.craziness1", 2567234);
	stat("test.some.craziness2", 2567234);
	stat("test.some.craziness3", 2567234);
	stat("test.some.craziness4", 2567234);
	stat("test.some.craziness5", 2567234);
}
