/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Events are stored in a prefix tree for fast, nice lookups.
 */
struct _ptrie {
	/**
	 * The character at this node
	 */
	gchar ch;

	/**
	 * Function called when client sends event to the server
	 */
	evs_handler handler;

	/**
	 * Called when client attempts to subscribe to event
	 */
	evs_subscribe subscribe;

	/**
	 * Called when client unsubscribes from event
	 */
	evs_unsubscribe unsubscribe;

	/**
	 * If this event wants to handle all children paths
	 */
	gboolean handle_children;

	/**
	 * All subscribers to this event, referenced by extra path segments
	 */
	GHashTable *subscribers;

	/**
	 * Any array of possible children.
	 * index 0 = '-'
	 */
	struct _ptrie *childs['z' - '-'];
};

static struct _ptrie _events;

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

static gboolean _ptrie_insert(
	const gchar *event_path,
	const evs_handler handler,
	const evs_subscribe subscribe,
	const evs_unsubscribe unsubscribe,
	const gboolean handle_children)
{
	gboolean inserted;
	const gchar *curr = event_path;
	struct _ptrie *parent = &_events;

	if (*curr == '/') {
		curr++;
	}

	while (*curr != '\0') {
		guchar ch = *curr - '-';
		struct _ptrie *child = parent->childs[ch];

		if (child == NULL) {
			child = g_slice_alloc0(sizeof(*child));
			child->ch = ch;

			// And with this one line, we can avoid all future locks!
			if (!__sync_bool_compare_and_swap(parent->childs + ch,
										NULL, child)) {
				g_slice_free1(sizeof(*child), child);
				child = parent->childs[ch];
				inserted = FALSE;
			}
		}

		parent = child;
		curr++;
	}

	if (!inserted) {
		return FALSE;
	}

	parent->handler = handler;
	parent->subscribe = subscribe;
	parent->unsubscribe = unsubscribe;
	parent->handle_children = handle_children;
	parent->subscribers = g_hash_table_new_full(g_str_hash,
										g_str_equal, NULL, NULL);

	return TRUE;
}

static struct _ptrie* _ptrie_lookup(
	const gchar *event_path,
	gchar **extra)
{
	const gchar *curr = event_path;
	struct _ptrie *parent = &_events;

	if (*curr == '/') {
		curr++;
	}

	while (*curr != '\0') {
		guchar ch = *curr - '-';
		struct _ptrie *child = parent->childs[ch];

		if (child == NULL) {
			if (parent->handle_children) {
				goto out;
			}

			return NULL;
		}

		parent = child;
		curr++;
	}

out:
	*extra = (gchar*)curr;
	return parent;
}

static void _clean_event_path(gchar *path, guint *len)
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

static void _ptrie_free(void *ptrie_)
{
	guint i;
	struct _ptrie *ptrie = ptrie_;

	if (ptrie == NULL) {
		return;
	}

	for (i = 0; i < G_N_ELEMENTS(ptrie->childs); i++) {
		_ptrie_free(ptrie->childs[i]);
	}

	g_slice_free1(sizeof(*ptrie), ptrie);
}

void evs_on(
	const gchar *prefix,
	const gchar *event_path,
	const evs_handler handler,
	const evs_subscribe subscribe,
	const evs_unsubscribe unsubscribe,
	const gboolean handle_children)
{
	guint len;
	gboolean created;
	GString *ep = qev_buffer_get();

	if (prefix != NULL) {
		g_string_append(ep, prefix);
		g_string_append_c(ep, '/');
	}

	g_string_append(ep, event_path);
	_clean_event_path(ep->str, &len);
	g_string_set_size(ep, len);

	created = _ptrie_insert(ep->str, handler, subscribe,
							unsubscribe, handle_children);
	if (!created) {
		WARN("Failed to create event %s: event already exists.", ep->str);
	}

	qev_buffer_put(ep);
}

void evs_route(
	struct client *client,
	gchar *event_path,
	const evs_cb_t client_callback,
	gchar *json)
{
	enum evs_status status = EVS_ERROR;
	gchar *extra = NULL;

	_clean_event_path(event_path, NULL);
	struct _ptrie *pevent = _ptrie_lookup(event_path, &extra);

	if (pevent == NULL) {
		DEBUG("Could not find handler for event: %s", event_path);
		goto out;
	}

	DEBUG("Got event: client=%p, event_path=%s, extra=%s, callback=%lu, json=%s",
			client, event_path, extra, client_callback, json);

	if (pevent->handler != NULL) {
		status = pevent->handler(client, extra);
	}

out:

	switch (status) {
		case EVS_OK:
			// evs_send_cb(client, client_callback, NULL, NULL);
			break;

		case EVS_ERROR:
			// evs_send_cb_err(client, client_callback, NULL, NULL);
			break;

		case EVS_HANDLED:
			break;
	}
}

void evs_init()
{
	evs_on(NULL, "///ev**ent///path///asdf/////", NULL, NULL, NULL, TRUE);
	qev_cleanup(&_events, _ptrie_free);
}
