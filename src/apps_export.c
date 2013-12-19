/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

void qio_export_add_handler(
	void *app_,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children)
{
	struct app *app = app_;
	GString *ep = qev_buffer_get();

	g_string_append_printf(ep, "%s/%s", app->prefix, ev_path);

	evs_add_handler(ep->str, handler_fn, subscribe_fn,
					unsubscribe_fn, handle_children);

	qev_buffer_put(ep);
}

void qio_export_send_cb(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json)
{
	evs_send_cb(client, client_cb, code, err_msg, json);
}

void qio_export_send_cb_full(
	struct client *client,
	const evs_cb_t client_cb,
	const enum evs_code code,
	const gchar *err_msg,
	const gchar *json,
	const evs_cb_fn cb_fn,
	void *cb_data,
	const qev_free_fn free_fn)
{
	evs_send_cb_full(client, client_cb, code, err_msg, json,
					cb_fn, cb_data, free_fn);
}

gboolean qio_export_json_unpack(
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

gboolean qio_export_json_pack(
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
