/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Essentially: [^_\-/a-zA-Z0-9]
 */
static const gchar _allowed_chars[256] = {
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  0,
	0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  0,  0,  0,  0,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	1,  1,  1,
};

static void _clean_ev_path(gchar *path, guint *len)
{
	gchar *curr = path;
	gchar *writing = path;

	while (*curr != '\0') {
		if (_allowed_chars[(guchar)*curr] && writing != curr) {
			if (!(*writing == '/' && *curr == '/')) {
				writing++;
			}

			*writing = *curr;
		}

		curr++;
	}

	// Remove any trailing slash
	if (*writing != '/') {
		writing++;
	}
	*writing = '\0';

	if (len != NULL) {
		*len = writing - path;
	}
}

void evs_on(
	const gchar *prefix,
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_subscribe_fn subscribe_fn,
	const evs_unsubscribe_fn unsubscribe_fn,
	const gboolean handle_children)
{
	guint len;
	gboolean created;
	GString *ep = qev_buffer_get();

	if (prefix != NULL) {
		g_string_append(ep, prefix);
		g_string_append_c(ep, '/');
	}

	g_string_append(ep, ev_path);
	_clean_ev_path(ep->str, &len);
	g_string_set_size(ep, len);

	created = evs_query_insert(ep->str, handler_fn, subscribe_fn,
							unsubscribe_fn, handle_children);
	if (!created) {
		WARN("Failed to create event %s: event already exists.", ep->str);
	}

	qev_buffer_put(ep);
}

void evs_route(
	struct client *client,
	gchar *ev_path,
	const evs_cb_t client_cb,
	gchar *json)
{
	struct event *ev;
	gchar *ev_extra = NULL;
	enum evs_status status = EVS_ERROR;

	_clean_ev_path(ev_path, NULL);

	ev = evs_query(ev_path, &ev_extra);
	if (ev == NULL) {
		DEBUG("Could not find handler for event: %s", ev_path);
		goto out;
	}

	CRITICAL("Got event: client=%p, ev_path=%s, ev_extra=%s, callback=%lu, json=%s",
			client, ev_path, ev_extra, client_cb, json);

	if (ev->handler_fn != NULL) {
		status = ev->handler_fn(client, ev_extra, client_cb, json);
	}

out:

	switch (status) {
		case EVS_OK:
			// evs_send_cb(client, client_cb, NULL, NULL);
			break;

		case EVS_ERROR:
			// evs_send_cb_err(client, client_cb, NULL, NULL);
			break;

		case EVS_HANDLED:
			break;
	}
}

void evs_subscribe(
	struct client *client,
	struct event *ev,
	const gchar *ev_extra,
	const evs_cb_t client_cb)
{
	DEBUG("Subscribing to: ev_path=%s, ev_extra=%s", ev->ev_path, ev_extra);
}

void evs_init()
{
	evs_query_init();
	evs_qio_init();
}
