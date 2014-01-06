/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

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

	return status == qev_json_ok;
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

	return status == qev_json_ok;
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
