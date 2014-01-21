/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

static void _replace(GString *str, gchar from, gchar to)
{
	guint i;
	for (i = 0; i < str->len; i++) {
		if (str->str[i] == from) {
			str->str[i] = to;
		}
	}
}

static GString* _stats_app_path(void *app_, const gchar *name)
{
	struct app *app = app_;
	GString *path = qev_buffer_get();

	g_string_printf(path, "/%s/%s", app->prefix, name);

	_replace(path, '.', '/');
	evs_clean_path(path->str);
	_replace(path, '/', '.');

	g_string_set_size(path, strnlen(path->str, path->len));

	if (*path->str == '.') {
		g_string_erase(path, 0, 1);
	}

	return path;
}

static struct event* _add_handler(
	void *app_,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children)
{
	struct event *ev;
	struct app *app = app_;
	GString *ep = qev_buffer_get();

	g_string_append_printf(ep, "%s/%s", app->prefix, ev_path);
	ev = evs_add_handler(ep->str, handler_fn, on_fn, off_fn, handle_children);
	qev_buffer_put(ep);

	return ev;
}

static qio_stats_gauge_t* _stats_gauge_monitor(
	void *app,
	const gchar *name,
	const qio_stats_monitor_fn monitor)
{
	void* stat;

	GString *path = _stats_app_path(app, name);
	stat = qev_stats_gauge_monitor(path->str, (qev_stats_monitor_fn)monitor);
	qev_buffer_put(path);

	return stat;
}

static qio_stats_gauge_t* _stats_gauge(void *app, const gchar *name)
{
	return _stats_gauge_monitor(app, name, NULL);
}

static qio_stats_counter_t* _stats_counter_monitor(
	void *app,
	const gchar *name,
	const gboolean reset_on_flush,
	const qio_stats_monitor_fn monitor)
{
	void *stat;

	GString *path = _stats_app_path(app, name);
	stat = qev_stats_counter_monitor(path->str, reset_on_flush,
								(qev_stats_monitor_fn)monitor);
	qev_buffer_put(path);

	return stat;
}

static qio_stats_counter_t* _stats_counter(
	void *app,
	const gchar *name,
	const gboolean reset_on_flush)
{
	return _stats_counter_monitor(app, name, reset_on_flush, NULL);
}

static qio_stats_timer_t* _stats_timer_monitor(
	void *app,
	const gchar *name,
	const qio_stats_monitor_fn monitor_sum,
	const qio_stats_monitor_fn monitor_stddev,
	const qio_stats_monitor_fn monitor_mean,
	const qio_stats_monitor_fn monitor_max,
	const qio_stats_monitor_fn monitor_min,
	const qio_stats_monitor_fn monitor_count)
{
	void *stat;

	GString *path = _stats_app_path(app, name);
	stat = qev_stats_timer_monitor(path->str,
						(qev_stats_monitor_fn)monitor_sum,
						(qev_stats_monitor_fn)monitor_stddev,
						(qev_stats_monitor_fn)monitor_mean,
						(qev_stats_monitor_fn)monitor_max,
						(qev_stats_monitor_fn)monitor_min,
						(qev_stats_monitor_fn)monitor_count);
	qev_buffer_put(path);

	return stat;
}

static qio_stats_timer_t* _stats_timer(void *app, const gchar *name)
{
	return _stats_timer_monitor(app, name, NULL, NULL, NULL, NULL, NULL, NULL);
}

static gboolean _json_unpack(
	gchar *json,
	const gchar *spec,
	...)
{
	va_list args;
	enum qev_json_status status;

	va_start(args, spec);
	status = qev_json_vunpack(json, NULL, spec, args);
	va_end(args);

	return status == QEV_JSON_OK;
}

static gboolean _json_pack(
	GString *buff,
	const gchar *spec,
	...)
{
	va_list args;
	enum qev_json_status status;

	va_start(args, spec);
	status = qev_json_vpack(buff, spec, args);
	va_end(args);

	return status == QEV_JSON_OK;
}

static struct qio_exports _exports = {
	.evs_add_handler = _add_handler,
	.evs_no_on = evs_no_on,
	.evs_send = evs_send,
	.evs_send_full = evs_send_full,
	.evs_on_cb = evs_on_cb,
	.evs_on_info_copy = evs_on_info_copy,
	.evs_on_info_free = evs_on_info_free,
	.evs_cb = evs_cb,
	.evs_cb_with_cb = evs_cb_with_cb,
	.evs_err_cb = evs_err_cb,
	.evs_cb_full = evs_cb_full,
	.evs_broadcast = evs_broadcast,
	.stats_gauge = _stats_gauge,
	.stats_gauge_monitor = _stats_gauge_monitor,
	.stats_gauge_set = (void*)qev_stats_gauge_set,
	.stats_gauge_get = (void*)qev_stats_gauge_get,
	.stats_counter = _stats_counter,
	.stats_counter_monitor = _stats_counter_monitor,
	.stats_counter_add = (void*)qev_stats_counter_add,
	.stats_counter_get = (void*)qev_stats_counter_get,
	.stats_timer = _stats_timer,
	.stats_timer_monitor = _stats_timer_monitor,
	.stats_timer_record = (void*)qev_stats_timer_record,
	.json_unpack = _json_unpack,
	.json_pack = _json_pack,
	.buffer_get = qev_buffer_get,
	.buffer_put = qev_buffer_put,
	.close = qev_close,
	.ref = qev_ref,
	.unref = qev_unref,
};

struct qio_exports apps_export_get_fns()
{
	return _exports;
}
