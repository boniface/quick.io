/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

// #define APP_ENABLE_DEBUG
#define G_LOG_DOMAIN "test_app_sane"

#include "../../include/quickio_app.h"
#include <stdio.h>

static GThread *_th = NULL;
static GAsyncQueue *_aq = NULL;
static gboolean _is_running = TRUE;

static event_t *_ev_with_send = NULL;

static gint _ons = 0;
static gint _offs = 0;

struct _work {
	GSourceFunc fn;
	void *data;
};

static void _do_work(GSourceFunc fn, void *data)
{
	struct _work w = {
		.fn = fn,
		.data = data,
	};
	g_async_queue_push(_aq, g_slice_copy(sizeof(w), &w));
}

static enum evs_status _stats_handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	GString *buff = qio.buffer_get();

	qio.json_pack(buff, "[%d,%d]", _ons, _offs);
	qio.evs_cb(client, client_cb, buff->str);

	qio.buffer_put(buff);

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_handler(
	client_t *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *val;
	gboolean good;

	good = qio.json_unpack(json, "{%s}", "key", &val);
	if (good && g_strcmp0(val, "value") == 0) {
		qio.evs_cb(client, client_cb, "\"test\"");
	} else {
		qio.evs_err_cb(client, client_cb, CODE_BAD, "Error with \"key\" in json.", NULL);
	}

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_on(const struct evs_on_info *info G_GNUC_UNUSED)
{
	g_atomic_int_inc(&_ons);
	return EVS_STATUS_OK;
}

static void _good_off(
	client_t *client G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED)
{
	g_atomic_int_inc(&_offs);
}

static gboolean _delayed_on_cb(void *info_)
{
	struct evs_on_info *info = info_;

	if (info->ev_extra == NULL) {
		qio.evs_on_cb(TRUE, info);
	} else {
		qio.evs_on_cb(g_strcmp0(info->ev_extra, "/child") == 0, info);
	}

	qio.evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _delayed_on(const struct evs_on_info *info)
{
	_do_work(_delayed_on_cb, qio.evs_on_info_copy(info, FALSE));
	return EVS_STATUS_HANDLED;
}

static enum evs_status _delayed_childs_on(const struct evs_on_info *info)
{
	_do_work(_delayed_on_cb, qio.evs_on_info_copy(info, TRUE));
	return EVS_STATUS_HANDLED;
}

static gboolean _delayed_reject_on_cb(void *info_)
{
	struct evs_on_info *info = info_;
	qio.evs_on_cb(FALSE, info);
	qio.evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _delayed_reject_on(const struct evs_on_info *info)
{
	_do_work(_delayed_reject_on_cb, qio.evs_on_info_copy(info, TRUE));
	return EVS_STATUS_HANDLED;
}

static gboolean _with_send_on_cb(void *info_)
{
	struct evs_on_info *info = info_;
	qio.evs_on_cb(TRUE, info);
	qio.evs_send(info->client, _ev_with_send, NULL, "\"with-send!\"");
	qio.evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _with_send_on(const struct evs_on_info *info)
{
	_do_work(_with_send_on_cb, qio.evs_on_info_copy(info, FALSE));
	return EVS_STATUS_HANDLED;
}

static void* _run(void *nothing G_GNUC_UNUSED)
{
	while (_is_running) {
		struct _work *w = g_async_queue_timeout_pop(_aq, 100);
		if (w == NULL) {
			continue;
		}

		/*
		 * Make this sleep a while so that all work is truly _delayed_.
		 */
		g_usleep(500);
		w->fn(w->data);
		g_slice_free1(sizeof(*w), w);
	}

	g_async_queue_unref(_aq);
	_aq = NULL;

	return NULL;
}

static gboolean _app_init()
{
	_aq = g_async_queue_new();
	_th = g_thread_new("test_app_sane", _run, NULL);

	qio.evs_add_handler(__qio_app, "/stats",
						_stats_handler, NULL, NULL, FALSE);
	qio.evs_add_handler(__qio_app, "/good",
						_good_handler, _good_on, _good_off, FALSE);
	qio.evs_add_handler(__qio_app, "/good2",
						_good_handler, _good_on, _good_off, FALSE);
	qio.evs_add_handler(__qio_app, "/delayed",
						NULL, _delayed_on, NULL, FALSE);
	qio.evs_add_handler(__qio_app, "/delayed-childs",
						NULL, _delayed_childs_on, NULL, TRUE);
	qio.evs_add_handler(__qio_app, "/reject",
						NULL, qio.evs_no_on, NULL, FALSE);
	qio.evs_add_handler(__qio_app, "/delayed-reject",
						NULL, _delayed_reject_on, NULL, FALSE);
	_ev_with_send = qio.evs_add_handler(__qio_app, "/with-send",
						NULL, _with_send_on, NULL, FALSE);

	return TRUE;
}

static gboolean _app_exit()
{
	_is_running = FALSE;

	g_thread_join(_th);
	_th = NULL;

	return TRUE;
}

static gboolean _app_test()
{
	printf("app test!\n");
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	_app_run,
	_app_exit,
	_app_test);
