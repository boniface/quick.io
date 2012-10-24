#include "qio.h"

/**
 * The message format to be sent to graphite, when there is an app prefix
 */
#define GRAPHITE_FORMAT "%s.%s:%d%s.%s %0.4f %" G_GINT64_FORMAT "\n"

/**
 * This is single-threaded, so let's give ourselves some working space
 */
static GString *_buffer;
static GString *_builder;

/**
 * The address structure indicating where UDP packets should be sent with Graphite info.
 */
static qsys_socket_t _graphite = -1;

/**
 * Address info for sending to graphite.
 */
struct sockaddr _graphite_addr;

/**
 * The hostname to send to graphite
 */
static gchar *_hostname = NULL;

/** 
 * Flush the stats currently in the buffer to graphite.
 */
static void _flush() {
	if (_buffer->len == 0) {
		return;
	}
	
	if (sendto(_graphite, _buffer->str, _buffer->len, 0, &_graphite_addr, sizeof(_graphite_addr)) == -1) {
		WARNF("Error sending stats to graphite: %s", strerror(errno));
	}
	
	g_string_truncate(_buffer, 0);
}

void stats_flush() {
	if (_graphite == -1) {
		return;
	}
	
	// Two special cases that are not reset from the stats object
	gint64 time = g_get_real_time() / 1000000;
	
	void _append(gchar *prefix, gchar *key, double val) {
		if (prefix == NULL) {
			prefix = "";
		}
		
		g_string_printf(_builder, GRAPHITE_FORMAT, option_stats_graphite_prefix(), _hostname, option_port(), prefix, key, val, time);
		
		// It's legal for the paths to graphite to be in the form qio.host/app/prefix.key,
		// so clean that up
		gchar *curr = _builder->str;
		do {
			if (*curr == '/') {
				*curr = '.';
			}
		} while (++curr && *curr != ' ' && *curr != '\0');
		
		if ((_buffer->len + _builder->len) > STATS_MAX_BUFFER_SIZE) {
			_flush();
		}
		
		g_string_append(_buffer, _builder->str);
	}
	
	// Just get the non-resetable stats
	#define X(slot, name) _append(NULL, name, (gsize)g_atomic_pointer_get(&(stats.slot)));
		STATS_S_VALUES
	#undef X
	
	// The counters have a few different stats we calculate, so we need this holder
	double val;
	
	#define X(slot, name) val = g_atomic_pointer_and(&(stats.slot), 0); _append(NULL, name ".count", val); _append(NULL, name ".mean", val / option_stats_flush());
		STATS_S_COUNTERS
	#undef X
	
	apps_stats_gather(_append);
	
	_flush();
}

gsize stats_clients() {
	return (gsize)g_atomic_pointer_get(&(stats.clients));
}

gboolean stats_init() {
	// Since graphite is optional, only emit a warning if anything fails
	if (option_stats_graphite_address() != NULL) {
		struct addrinfo *res;
		
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		
		gchar port[6];
		snprintf(port, sizeof(port), "%d", option_stats_graphite_port());
		
		// Holy fuck! Setting up a UDP port is just messy
		if (getaddrinfo(option_stats_graphite_address(), port, &hints, &res) != 0) {
			WARN("Could not lookup get address info for graphite server. Graphite will not be used.");
		} else {
			// Go through the results until one works
			struct addrinfo *record;
			for (record = res; record != NULL; record = record->ai_next) {
				if ((_graphite = socket(record->ai_family, record->ai_socktype, record->ai_protocol)) == -1) {
					continue;
				}
				
				gchar hostname[1024];
				memset(&hostname, 0, sizeof(hostname));
				if (gethostname(hostname, sizeof(hostname)-1) == -1) {
					WARN("Could not determine hostname, falling back to bind-address");
					_hostname = g_strdup(option_bind_address());
				} else {
					_hostname = g_strdup(hostname);
				}
				
				_graphite_addr = *(record->ai_addr);
				break;
			}
			
			if (_graphite == -1) {
				WARNF("Could not create UDP socket for graphite: %s", strerror(errno));
			}
			
			freeaddrinfo(res);
		}
	}
	
	_buffer = g_string_sized_new(128);
	_builder = g_string_sized_new(128);
	
	return TRUE;
}

#ifdef TESTING
#include "../test/test_stats.c"
#endif