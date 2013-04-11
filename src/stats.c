#include "qio.h"

/**
 * The message format to be sent to graphite, when there is an app prefix
 */
#define GRAPHITE_FORMAT "%s.%s%s.%s %0.4f %" G_GINT64_FORMAT "\n"

/**
 * Timers only fire in 1 thread, so this is safe
 */
static GString *_buffer;
static GString *_builder;

/**
 * The address structure indicating where UDP packets should be sent with Graphite info.
 */
static rsocket *_graphite = NULL;

void stats_flush() {
	if (!_graphite) {
		return;
	}

	gint64 time = qev_time;

	void _append(gchar *prefix, gchar *key, double val) {
		if (prefix == NULL) {
			prefix = "";
		}

		g_string_printf(_builder, GRAPHITE_FORMAT, option_stats_graphite_prefix(), option_hostname(), prefix, key, val, time);

		// It's legal for the paths to graphite to be in the form qio.host/app/prefix.key,
		// so clean that up
		gchar *curr = _builder->str;
		do {
			if (*curr == '/') {
				*curr = '.';
			}
		} while (++curr && *curr != ' ' && *curr != '\0');

		g_string_append(_buffer, _builder->str);
	}

	// Just get the non-resetable stats
	#define X(slot, name) _append(NULL, name, (gsize)g_atomic_pointer_get(&(stats.slot)));
		STATS_S_VALUES
	#undef X

	// The counters have a few different stats we calculate, so we need this holder
	double val;

	#define X(slot, name) val = g_atomic_pointer_and(&(stats.slot), 0); _append(NULL, name ".count", val); _append(NULL, name ".mean", val / STATS_INTERVAL);
		STATS_S_COUNTERS
	#undef X

	apps_stats_gather(_append);

	rsocket_send(_graphite, _buffer->str, _buffer->len);
	g_string_truncate(_buffer, 0);
}

gsize stats_clients() {
	return (gsize)g_atomic_pointer_get(&(stats.clients));
}

gboolean stats_init() {
	// Since graphite is optional, only emit a warning if anything fails
	if (option_stats_graphite_address() != NULL) {
		if (rsocket_connect(option_stats_graphite_address(), option_stats_graphite_port(), &_graphite) == -1) {
			WARN("Could not lookup graphite address. Graphite will not be used.");
		}
	}

	_buffer = g_string_sized_new(128);
	_builder = g_string_sized_new(128);

	return TRUE;
}

#ifdef TESTING
#include "../test/test_stats.c"
#endif
