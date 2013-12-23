/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * The internal ptrie node
 */
struct _ptrie {
	/**
	 * The actual event that lives at this node.
	 */
	struct event ev;

	/**
	 * The character at this node
	 */
	gchar ch;

	/**
	 * Any array of possible children.
	 * index 0 = '-'
	 */
	struct _ptrie *childs['z' - '-'];
};

static struct _ptrie _events;

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

	if (ptrie->ev.ev_path != NULL) {
		event_clear(&ptrie->ev);
	}

	g_slice_free1(sizeof(*ptrie), ptrie);
}

struct event* evs_query_insert(
	const gchar *ev_path,
	const evs_handler_fn handler_fn,
	const evs_on_fn on_fn,
	const evs_off_fn off_fn,
	const gboolean handle_children)
{
	const gchar *curr = ev_path;
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
			}
		}

		parent = child;
		curr++;
	}

	if (parent->ev.ev_path != NULL) {
		return NULL;
	}

	event_init(&parent->ev, ev_path, handler_fn, on_fn, off_fn, handle_children);

	return &parent->ev;
}

struct event* evs_query(
	const gchar *ev_path,
	gchar **ev_extra)
{
	const gchar *curr = ev_path;
	struct _ptrie *parent = &_events;

	if (*curr == '/') {
		curr++;
	}

	while (*curr != '\0') {
		guchar ch = *curr - '-';
		struct _ptrie *child = parent->childs[ch];

		if (child == NULL) {
			if (parent->ev.handle_children) {
				goto out;
			}

			return NULL;
		}

		parent = child;
		curr++;
	}

out:
	*ev_extra = (gchar*)curr;
	return &parent->ev;
}

void evs_query_init()
{
	qev_cleanup(&_events, _ptrie_free);
}
