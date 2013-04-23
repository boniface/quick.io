#include "../app/qio_app.h"

static GAsyncQueue *_work;

typedef struct {
	client_t *client;
	event_handler_t *handler;
	path_extra_t *extra;
	callback_t client_callback;
	gboolean success;
} work_t;

static status_t _test_handler(client_t *client, event_handler_t *handler, event_t *event, GString *response) {
	TEST_STATS_INC(apps_client_handler);
	return CLIENT_GOOD;
}

static status_t _test_handler_on(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback) {
	TEST_STATS_INC(apps_client_handler_on);

	return CLIENT_GOOD;
}

static status_t _test_handler_on_reject(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback) {
	return CLIENT_ERROR;
}

static status_t _test_handler_on_async(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback) {
	work_t *work = g_slice_alloc0(sizeof(*work));
	work->client = client;
	work->handler = (event_handler_t*)handler;
	work->extra = extra;
	work->client_callback = client_callback;
	work->success = TRUE;

	client_ref(client);
	g_ptr_array_ref(extra);

	g_async_queue_push(_work, work);

	return CLIENT_ASYNC;
}

static status_t _test_handler_on_async_reject(client_t *client, const event_handler_t *handler, path_extra_t *extra, const callback_t client_callback) {
	work_t *work = g_slice_alloc0(sizeof(*work));
	work->client = client;
	work->handler = (event_handler_t*)handler;
	work->extra = extra;
	work->client_callback = client_callback;
	work->success = FALSE;

	client_ref(client);
	g_ptr_array_ref(extra);

	g_async_queue_push(_work, work);

	return CLIENT_ASYNC;
}

static void _test_handler_off(client_t *client, const event_handler_t *handler, path_extra_t *extra) {
	TEST_STATS_INC(apps_client_handler_off);
}

gboolean app_init(app_on on) {
	_work = g_async_queue_new();

	on("/event", _test_handler, _test_handler_on, _test_handler_off, TRUE);
	on("/children/reject", NULL, _test_handler_on_reject, NULL, TRUE);
	on("/async", NULL, _test_handler_on_async, NULL, TRUE);
	on("/async/reject", NULL, _test_handler_on_async_reject, NULL, TRUE);
	return TRUE;
}

gboolean app_run() {
	TEST_STATS_INC(apps_run);

	while (TRUE) {
		work_t *work = g_async_queue_pop(_work);

		evs_client_app_sub_cb(work->client, work->handler, work->extra, work->client_callback, work->success);
		client_unref(work->client);
		g_ptr_array_unref(work->extra);

		g_slice_free1(sizeof(*work), work);
	}

	return TRUE;
}

void app_client_connect(client_t *client) {
	TEST_STATS_INC(apps_client_connect);

	client_set(client, "test", "test");
}

void app_client_close(client_t *client) {
	TEST_STATS_INC(apps_client_close);

	// Access data on the client to ensure it's accessible to callbacks
	client_has(client, "test");
	client_get(client, "test");
}

void app_evs_client_subscribe(client_t *client, const gchar *event_path, path_extra_t *extra, const guint extra_len) {
	TEST_STATS_INC(apps_client_subscribe);
}

void app_evs_client_unsubscribe(client_t *client, const gchar *event_path, path_extra_t *extra, const guint extra_len) {
	TEST_STATS_INC(apps_client_unsubscribe);
}

void app_stats_flush(apps_stats_cb stat) {
	stat("test.key", 2);
	stat("test.some.craziness0", 2567234);
	stat("test.some.craziness1", 2567234);
	stat("test.some.craziness2", 2567234);
	stat("test.some.craziness3", 2567234);
	stat("test.some.craziness4", 2567234);
	stat("test.some.craziness5", 2567234);
}
