/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

// #define APP_ENABLE_DEBUG
#define G_LOG_DOMAIN "test_app_sane"
#include "quickio.h"
#define EV_PREFIX "/test"
#define STATS_PREFIX "test"

static GThread *_th = NULL;
static GAsyncQueue *_aq = NULL;
static gboolean _is_running = TRUE;

static struct event *_ev_with_send = NULL;

static qev_stats_gauge_t *_gauge = NULL;
static qev_stats_gauge_t *_gauge_mon = NULL;

static qev_stats_counter_t *_ons = NULL;
static qev_stats_counter_t *_offs = NULL;

static qev_stats_timer_t *_timer = NULL;
static qev_stats_timer_t *_timer_mon = NULL;

static gint64 _sane_value = 0;

struct _work {
	GSourceFunc fn;
	void *data;
};

static struct qev_cfg _cfg[] = {
	{	.name = "sane-value",
		.description = "A sane value",
		.type = QEV_CFG_INT64,
		.val.i64 = &_sane_value,
		.defval.i64 = 10,
		.validate = NULL,
		.cb = NULL,
		.read_only = TRUE,
	},
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
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json G_GNUC_UNUSED)
{
	GString *buff = qev_buffer_get();

	qev_stats_gauge_set(_gauge, 10);
	qev_stats_gauge_set(_gauge_mon, 20);

	qev_stats_time(_timer, {
		g_usleep(10);
	});

	qev_stats_time(_timer_mon, {
		g_usleep(10);
	});

	qev_json_pack(buff, "[%d,%d,%d,%d,%d]",
					qev_stats_counter_get(_ons),
					qev_stats_counter_get(_offs),
					qev_stats_gauge_get(_gauge),
					qev_stats_gauge_get(_gauge_mon),
					_sane_value);
	evs_cb(client, client_cb, buff->str);

	qev_buffer_put(buff);

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_handler(
	struct client *client,
	const gchar *ev_extra G_GNUC_UNUSED,
	const evs_cb_t client_cb,
	gchar *json)
{
	gchar *val;
	guint parsed;
	enum qev_json_status jstatus;

	jstatus = qev_json_unpack(json, &parsed, "{%s}", "key", &val);
	if (jstatus == QEV_JSON_OK && g_strcmp0(val, "value") == 0) {
		evs_cb(client, client_cb, "\"test\"");
	} else {
		evs_err_cb(client, client_cb, CODE_BAD, "Error with \"key\" in json.", NULL);
	}

	return EVS_STATUS_HANDLED;
}

static enum evs_status _good_on(const struct evs_on_info *info G_GNUC_UNUSED)
{
	qev_stats_counter_inc(_ons);
	return EVS_STATUS_OK;
}

static void _good_off(
	struct client *client G_GNUC_UNUSED,
	const gchar *ev_extra G_GNUC_UNUSED)
{
	qev_stats_counter_inc(_offs);
}

static gboolean _delayed_on_cb(void *info_)
{
	struct evs_on_info *info = info_;

	if (info->ev_extra == NULL) {
		evs_on_cb(TRUE, info);
	} else {
		evs_on_cb(g_strcmp0(info->ev_extra, "/child") == 0, info);
	}

	evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _delayed_on(const struct evs_on_info *info)
{
	_do_work(_delayed_on_cb, evs_on_info_copy(info, FALSE));
	return EVS_STATUS_HANDLED;
}

static enum evs_status _delayed_childs_on(const struct evs_on_info *info)
{
	_do_work(_delayed_on_cb, evs_on_info_copy(info, TRUE));
	return EVS_STATUS_HANDLED;
}

static gboolean _delayed_reject_on_cb(void *info_)
{
	struct evs_on_info *info = info_;
	evs_on_cb(FALSE, info);
	evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _delayed_reject_on(const struct evs_on_info *info)
{
	_do_work(_delayed_reject_on_cb, evs_on_info_copy(info, TRUE));
	return EVS_STATUS_HANDLED;
}

static gboolean _with_send_on_cb(void *info_)
{
	struct evs_on_info *info = info_;
	evs_on_cb(TRUE, info);
	evs_send(info->client, _ev_with_send, NULL, "\"with-send!\"");
	evs_on_info_free(info);
	return G_SOURCE_REMOVE;
}

static enum evs_status _with_send_on(const struct evs_on_info *info)
{
	_do_work(_with_send_on_cb, evs_on_info_copy(info, FALSE));
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
	_gauge = qev_stats_gauge(STATS_PREFIX, "gauge");
	_gauge_mon = qev_stats_gauge_monitor(STATS_PREFIX, "gauge_mon", NULL);

	_ons = qev_stats_counter(STATS_PREFIX, "ons", FALSE);
	_offs = qev_stats_counter_monitor(STATS_PREFIX, "offs", FALSE, NULL);

	_timer = qev_stats_timer(STATS_PREFIX, "timer");
	_timer_mon = qev_stats_timer_monitor(STATS_PREFIX, "timer_mon",
									NULL, NULL, NULL,
									NULL, NULL, NULL);

	_aq = g_async_queue_new();
	_th = g_thread_new("test_app_sane", _run, NULL);

	evs_add_handler(EV_PREFIX, "/stats",
						_stats_handler, NULL, NULL, FALSE);
	evs_add_handler(EV_PREFIX, "/good",
						_good_handler, _good_on, _good_off, FALSE);
	evs_add_handler(EV_PREFIX, "/good-childs",
						_good_handler, _good_on, _good_off, TRUE);
	evs_add_handler(EV_PREFIX, "/good2",
						_good_handler, _good_on, _good_off, FALSE);
	evs_add_handler(EV_PREFIX, "/delayed",
						NULL, _delayed_on, NULL, FALSE);
	evs_add_handler(EV_PREFIX, "/delayed-childs",
						NULL, _delayed_childs_on, NULL, TRUE);
	evs_add_handler(EV_PREFIX, "/reject",
						NULL, evs_no_on, NULL, FALSE);
	evs_add_handler(EV_PREFIX, "/delayed-reject",
						NULL, _delayed_reject_on, NULL, FALSE);
	_ev_with_send = evs_add_handler(EV_PREFIX, "/with-send",
						NULL, _with_send_on, NULL, FALSE);

	qev_cfg_add("test_app_sane", _cfg, G_N_ELEMENTS(_cfg));

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
	return TRUE;
}

QUICKIO_APP(
	_app_init,
	_app_exit);

QUICKIO_APP_TEST(_app_test);
