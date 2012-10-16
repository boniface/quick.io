#include "qio.h"

#define RESPONSE "HTTP/1.0 200 OK\r\n" \
	"Content-Type: application/json\r\n" \
	"Expires: -1\r\n\r\n"

#define RECORD_FORMAT "{" \
	"\"time\":%" G_GSIZE_FORMAT "," \
	"\"clients\":%" G_GSIZE_FORMAT "," \
	"\"messages_sent\":%" G_GSIZE_FORMAT "," \
	"\"messages_received\":%" G_GSIZE_FORMAT "},"

/** 
 * The FD pointing to the shared memory.
 */
static int _statsfd;

/**
 * The list of stats for the past option_stats_flush() time.
 */
static stats_t *_historical;

/**
 * The current record in _historical being filled
 */
static guint64 _historical_record = 0;

/**
 * This is single-threaded, so let's just store the response buffer
 */
static GString *_buffer;

/**
 * The address structure indicating where UDP packets should be sent with Graphite info.
 */
static qsys_socket_t _graphite = -1;

/**
 * Address info for sending to graphite.
 */
struct addrinfo _graphite_addr;

/**
 * The hostname to send to graphite
 */
static gchar *_hostname = NULL;

void stats_tick() {
	static gsize ticks = 0;
	
	stats_t *curr = (_historical + (_historical_record++ % option_stats_flush()));
	
	// Two special cases that are not reset from the stats object
	GTimeVal time;
	g_get_current_time(&time);
	curr->time = time.tv_sec;
	curr->clients = stats->clients;
	
	// Copy over the current stats to the history
	#define X(type, name) curr->name = g_atomic_pointer_and(&(stats->name), 0);
		STATS_S_FIELDS_RESETABLE
	#undef X
	
	// Tick is hit once per second, so...
	if ((++ticks % option_stats_flush()) == 0 && _graphite != -1) {
		ticks = 0;
		g_string_truncate(_buffer, 0);
		
		// Sum up the current historical stats for sending
		stats_t sum;
		memset(&sum, 0, sizeof(sum));
		for (guint64 i = 0; i < option_stats_flush(); i++) {
			#define X(type, name) sum.name += (_historical + i)->name;
				STATS_S_FIELDS_RESETABLE
			#undef X
		}
		
		// Special case stat
		sum.clients = stats->clients;
		
		// Construct the graphite message
		#define X(type, name) \
			g_string_append_printf(_buffer, "%s.%s.%s %" G_GSIZE_FORMAT " %" G_GSIZE_FORMAT "\n", \
				option_stats_graphite_prefix(), \
				_hostname, \
				#name, \
				sum.name, \
				time.tv_sec \
			);
			STATS_S_GRAPHITE
		#undef X
		
		if (sendto(_graphite, _buffer->str, _buffer->len, 0, _graphite_addr.ai_addr, _graphite_addr.ai_addrlen) == -1) {
			WARNF("Error sending stats to graphite: %s", strerror(errno));
		}
	}
}

void stats_request(qsys_socket_t sock) {
	g_string_assign(_buffer, RESPONSE);
	
	g_string_append_c(_buffer, '[');
	
	// Go through the records in reverse, putting the newest on top
	for (guint64 i = 0; i < option_stats_flush(); i++) {
		stats_t *curr = _historical + i;
		
		g_string_append_printf(_buffer, RECORD_FORMAT,
			curr->time,
			curr->clients,
			curr->messages_sent,
			curr->messages_received
		);
	}
	
	g_string_overwrite(_buffer, _buffer->len-1, "]");
	
	send(sock, _buffer->str, _buffer->len, MSG_NOSIGNAL);
}

gboolean stats_init() {
	_statsfd = shm_open(SHM_NAME, SHM_FLAGS, SHM_MASK);
	if (_statsfd == -1) {
		ERRORF("Could not open shared memory region for stats: %s", strerror(errno));
		return FALSE;
	}
	
	if (ftruncate(_statsfd, sizeof(*stats)) == -1) {
		ERRORF("Could not allocate memory for stats: %s", strerror(errno));
		return FALSE;
	}
	
	stats = mmap(NULL, sizeof(*stats), PROT_READ | PROT_WRITE, MAP_SHARED, _statsfd, 0);
	if (stats == (void*)-1) { // Suppress the gcc warning
		ERRORF("Could not mmap stats memory: %s", strerror(errno));
		return FALSE;
	}
	
	memset(stats, 0, sizeof(*stats));
	
	_historical = g_malloc0(sizeof(*_historical) * option_stats_flush());
	_buffer = g_string_sized_new(128);
	
	// Since graphite is optional, only emit a warning if anything fails
	if (option_stats_graphite_address() != NULL) {
		int status;
		struct addrinfo *res;
		
		struct addrinfo hints;
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		
		gchar port[6];
		snprintf(port, sizeof(port), "%d", option_stats_graphite_port());
		
		// Holy fuck! Setting up a UDP port is just messy
		if ((status = getaddrinfo(option_stats_graphite_address(), port, &hints, &res)) != 0) {
			WARN("Could not lookup IP for graphite server. Graphite not being used.");
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
				
				_graphite_addr = *record;
				break;
			}
			
			if (_graphite == -1) {
				WARNF("Could not create UDP socket for graphite: %s", strerror(errno));
			}
			
			freeaddrinfo(res);
		}
	}
	
	return TRUE;
}

#ifdef TESTING
#include "../test/test_stats.c"
#endif