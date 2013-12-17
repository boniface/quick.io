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
	GString *ep = g_string_new(app->prefix);

	g_string_append_printf(ep, "/%s", ev_path);

	evs_add_handler(ep->str, handler_fn, subscribe_fn,
					unsubscribe_fn, handle_children);

	g_string_free(ep, TRUE);
}
