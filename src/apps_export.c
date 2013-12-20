/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

struct event* qio_export_add_handler(
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

GString* qio_export_buffer_get()
{
	return qev_buffer_get();
}

void qio_export_buffer_put(GString *buff)
{
	qev_buffer_put(buff);
}
