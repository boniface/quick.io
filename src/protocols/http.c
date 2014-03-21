/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * Header key that defines the version of websocket being used
 */
#define HTTP_VERSION_KEY "Sec-WebSocket-Version"

/**
 * The header key that points to the challenge key
 */
#define HTTP_CHALLENGE_KEY "Sec-WebSocket-Key"

/**
 * The header key that points to what subprotocol is being used
 */
#define HTTP_PROTOCOL_KEY "Sec-WebSocket-Protocol"

/**
 * Where HTTP stores its `Connection` header
 */
#define HTTP_CONNECTION "Connection"

/**
 * Where HTTP stores its `Upgrade` header
 */
#define HTTP_UPGRADE "Upgrade"

/**
 * Where HTTP stores its `Content-Length` header
 */
#define HTTP_CONTENT_LENGTH "Content-Length"

/**
 * What the client says its ID is
 */
#define HTTP_CONN_ID "X-QIO-Conn-Id"

#define STATUS_200 "200 OK"
#define STATUS_400 "400 Bad Request"
#define STATUS_403 "403 Forbidden"
#define STATUS_405 "405 Method Not Allowed"
#define STATUS_411 "411 Length Required"
#define STATUS_413 "413 Request Entity Too Large"
#define STATUS_426 "426 Upgrade Required"

#define HTTP_RESPONSE \
	"HTTP/1.1 %s\r\n" \
	HTTP_COMMON \
	"Content-Length: "

#define HTTP_HEADER_TERMINATOR "\r\n\r\n"
#define HTTP_HEADER_TERMINATOR2 "\n\n"

struct client_table {
	GHashTable *tbl;
	qev_lock_t lock;
};

static qev_stats_counter_t *_stat_handshakes_http;
static qev_stats_counter_t *_stat_handshakes_http_invalid;
static qev_stats_counter_t *_stat_handshakes_http_missing_upgrade;

static struct client_table _clients[64];

/**
 * From: http://stackoverflow.com/questions/2673207/c-c-url-decode-library
 */
// static void _urldecode(gchar *str)
// {
// 	const gchar *src = str;

// 	while (*src) {
// 		gchar a, b;

// 		if (*src == '%' && (a = src[1]) && (b = src[2]) &&
// 			g_ascii_isxdigit(a) && g_ascii_isxdigit(b)) {

// 			if (a >= 'a') {
// 				a -= 'a' - 'A';
// 			}

// 			if (a >= 'A') {
// 				a -= 'A' - 10;
// 			} else {
// 				a -= '0';
// 			}

// 			if (b >= 'a') {
// 				b -= 'a' - 'A';
// 			}

// 			if (b >= 'A') {
// 				b -= 'A' - 10;
// 			} else {
// 				b -= '0';
// 			}

// 			*str++ = 16 * a + b;
// 			src += 3;
// 		} else {
// 			*str++ = *src++;
// 		}
// 	}

// 	*str++ = '\0';
// }

static guint _uint128_hash(const void *i_)
{
	__uint128_t i = *(__uint128_t*)i_;
	guint64 upper = i >> 64;
	guint64 lower = (guint64)i;
	return upper ^ lower;
}

static gboolean _uint128_equal(const void *i1_, const void *i2_)
{
	__uint128_t i1 = *(__uint128_t*)i1_;
	__uint128_t i2 = *(__uint128_t*)i2_;
	return i1 == i2;
}

static gboolean _has_complete_header(struct client *client)
{
	GString *rbuff = client->qev_client.rbuff;

	return strstr(rbuff->str, HTTP_HEADER_TERMINATOR) != NULL ||
		strstr(rbuff->str, HTTP_HEADER_TERMINATOR) != NULL;
}

static gchar* _find_header_end(gchar *head)
{
	gchar *end;

	end = strstr(head, HTTP_HEADER_TERMINATOR);
	if (end != NULL) {
		end += strlen(HTTP_HEADER_TERMINATOR);
		*(end - 1) = '\0';
		return end;
	}

	end = strstr(head, HTTP_HEADER_TERMINATOR2);
	if (end != NULL) {
		end += strlen(HTTP_HEADER_TERMINATOR2);
		*(end - 1) = '\0';
		return end;
	}

	return NULL;
}

static struct client* _get_surrogate(gchar *url)
{
	union {
		uuid_t u;
		__uint128_t i;
	} uuid;
	struct client *surrogate;
	struct client_table *tbl;
	guint which;
	gchar *sid = strstr(url, "sid=");
	GString *buff = qev_buffer_get();

	g_string_assign(buff, url);

	// @todo test case for /connect=true/?sid= <- even worth caring about?
	gboolean new_client = strstr(url, "connect=true") != NULL;

	if (sid == NULL) {
		INFO("Failed to find sid: %s", url);
		return NULL;
	}

	sid += 4;
	strtok(sid, " &");
	if (uuid_parse(sid, uuid.u) != 0) {
		INFO("Failed to parse sid: %s", sid);
		return NULL;
	}

	which = uuid.i % G_N_ELEMENTS(_clients);
	tbl = _clients + which;

	qev_lock_read_lock(&tbl->lock);
	surrogate = g_hash_table_lookup(tbl->tbl, &uuid.i);
	qev_lock_read_unlock(&tbl->lock);

	if (surrogate == NULL && new_client) {
		qev_lock_write_lock(&tbl->lock);

		surrogate = g_hash_table_lookup(tbl->tbl, &uuid.i);
		if (surrogate == NULL) {
			surrogate = protocols_new_surrogate(protocol_http);
			surrogate->http.sid = uuid.i;
			surrogate->http.tbl = which;
			g_hash_table_insert(tbl->tbl, &surrogate->http.sid, surrogate);
		}

		qev_lock_write_unlock(&tbl->lock);
	}

	INFO("Url: %s (which=%u)", buff->str, which);
	if (surrogate == NULL) {
		FATAL("Got NULL surrogate");
	}

	qev_buffer_put(buff);

	return surrogate;
}

static GString* _build_response(const gchar *status, const GString *body)
{
	GString *buff = qev_buffer_get();
	g_string_printf(buff, HTTP_RESPONSE, status);

	if (body == NULL || body->len == 0) {
		qev_buffer_append_uint(buff, 0);
		g_string_append(buff, "\r\n\r\n");
	} else {
		qev_buffer_append_uint(buff, body->len);
		g_string_append(buff, "\r\n\r\n");
		qev_buffer_append_buff(buff, body);
	}

	return buff;
}

/**
 * Sends a full HTTP response to the client (not a surrogate).
 */
static void _send_response(struct client *client, GString *resp)
{
	qev_lock(client);

	if (!client->http.flags.responded) {
		client->http.flags.responded = TRUE;

		resp->str[strlen("HTTP/1.")] = client->http.flags.http_11 ? '1' : '0';
		qev_write(client, resp->str, resp->len);

		if (!client->http.flags.keep_alive) {
			qev_close(client, HTTP_DONE);
		}

		client->http.client = NULL;
	}

	qev_unlock(client);
}

/**
 * A shortcut for sending a response to a client, not a surrogate
 */
static void _send(
	struct client *client,
	const gchar *status,
	const GString *body)
{
	if (client->http.flags.responded) {
		return;
	}

	GString *buff = _build_response(status, body);
	_send_response(client, buff);
	qev_buffer_put(buff);
}

static void _handle_body(struct client *client, gchar *start)
{
	gchar *saveptr = NULL;

	if (client->http.flags.iframe_requested) {

	} else if (!client->http.flags.is_post) {
		_send(client, STATUS_405, NULL);
	} else if (client->http.client == NULL) {
		/*
		 * No surrogate was found, and the body has arrived, so just ignore
		 * everything (since there's nothing to be done with it), and send
		 * back an error.
		 */
		FATAL("SENDING 403");
		_send(client, STATUS_403, NULL);
	} else {
		GString *msgs;
		struct client *surrogate = client->http.client;

		surrogate->http.flags.incoming = TRUE;

		while (TRUE) {
			gchar *msg = strtok_r(start, "\n", &saveptr);
			start = NULL;
			if (msg == NULL) {
				break;
			}

			protocol_raw_handle(surrogate, msg);
		}

		qev_lock(surrogate);

		surrogate->http.flags.incoming = FALSE;

		msgs = qev_surrogate_flush(surrogate);

		if (surrogate->http.client == NULL) {
			// If nothing is ready, just let the client wait for messages
			// rather than responding immediately, thus causing another poll
			if (msgs == NULL) {
				surrogate->http.client = client;
			} else {
				_send(client, STATUS_200, msgs);
			}
		} else {
			// Even if nothing is ready, this connection is replacing a previous
			// one, so flush out whatever was gotten and set the new client
			_send(surrogate->http.client, STATUS_200, msgs);
			surrogate->http.client = client;
		}

		qev_unlock(surrogate);

		qev_buffer_put(msgs);
	}
}

static void _do_websocket_upgrade(
	struct client *client,
	const struct protocol_headers *headers,
	const gchar *connection,
	const gchar *key)
{
	gchar *protocol = protocol_util_headers_get(headers, HTTP_PROTOCOL_KEY);
	gchar *upgrade = protocol_util_headers_get(headers, HTTP_UPGRADE);
	gchar *version = protocol_util_headers_get(headers, HTTP_VERSION_KEY);

	if (upgrade == NULL ||
		connection == NULL ||
		strcasestr(connection, "Upgrade") == NULL) {

		qev_stats_counter_inc(_stat_handshakes_http_missing_upgrade);
		_send(client, STATUS_426, 0);
	} else if (key == NULL ||
		g_strcmp0(protocol, "quickio") != 0 ||
		g_strcmp0(version, "13") != 0) {

		qev_stats_counter_inc(_stat_handshakes_http_invalid);
		_send(client, STATUS_400, 0);
	} else {
		protocol_rfc6455_upgrade(client, key);
	}
}

static enum protocol_status _do_http(
	struct client *client,
	const struct protocol_headers *headers,
	gchar *head_start)
{
	gchar *content_len;
	guint64 body_len = 0;
	gboolean is_post = g_str_has_prefix(head_start, "POST");
	gchar *url = strstr(head_start, "/");

	if (url == NULL) {
		goto fatal;
	}

	content_len = protocol_util_headers_get(headers, HTTP_CONTENT_LENGTH);
	if (content_len != NULL) {
		gchar *len_end;
		body_len = g_ascii_strtoull(content_len, &len_end, 10);
		if (body_len == 0 && len_end == content_len) {
			goto fatal;
		}
	} else if (is_post) {
		qev_close(client, HTTP_LENGTH_REQUIRED);
		return PROT_FATAL;
	}

	client->http.client = _get_surrogate(url);
	client->http.body_len = body_len;
	client->http.flags.is_post = is_post;
	client->http.flags.iframe_requested = g_str_has_prefix(url, "/iframe");
	client->http.flags.responded = FALSE;

	return PROT_OK;

fatal:
	qev_close(client, HTTP_BAD_REQUEST);
	return PROT_FATAL;
}

static void _cleanup()
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(_clients); i++) {
		g_hash_table_unref(_clients[i].tbl);
		_clients[i].tbl = NULL;
	}
}

void protocol_http_init()
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(_clients); i++) {
		_clients[i].tbl = g_hash_table_new(_uint128_hash, _uint128_equal);
	}

	qev_cleanup_fn(_cleanup);

	_stat_handshakes_http = qev_stats_counter("protocol.http",
								"handshakes.http", TRUE);
	_stat_handshakes_http_invalid = qev_stats_counter("protocol.http",
								"handshakes.http_invalid", TRUE);
	_stat_handshakes_http_missing_upgrade = qev_stats_counter("protocol.http",
								"handshakes.http_missing_upgrade", TRUE);
}

enum protocol_handles protocol_http_handles(struct client *client)
{
	GString *rbuff = client->qev_client.rbuff;

	if (!_has_complete_header(client)) {
		return PROT_MAYBE;
	}

	if (g_str_has_prefix(rbuff->str, "GET /") ||
		g_str_has_prefix(rbuff->str, "POST /") ||
		g_str_has_prefix(rbuff->str, "OPTIONS /") ||
		g_str_has_prefix(rbuff->str, "PUT /") ||
		g_str_has_prefix(rbuff->str, "HEAD /") ||
		g_str_has_prefix(rbuff->str, "DELETE /")) {
		return PROT_YES;
	}

	return PROT_NO;
}

enum protocol_status protocol_http_route(struct client *client, gsize *used)
{
	struct protocol_headers headers;
	enum protocol_status status = PROT_OK;
	GString *rbuff = client->qev_client.rbuff;

	if (client->http.body_len == 0) {
		gchar *key;
		gchar *connection;
		gboolean is_http_11;
		gboolean keep_alive;
		gchar *head_start = rbuff->str + *used;
		gchar *body_start = NULL;

		body_start = _find_header_end(head_start);
		if (body_start == NULL) {
			return PROT_AGAIN;
		}

		protocol_util_headers(head_start, &headers);
		key = protocol_util_headers_get(&headers, HTTP_CHALLENGE_KEY);
		connection = protocol_util_headers_get(&headers, HTTP_CONNECTION);

		is_http_11 = strstr(head_start, "HTTP/1.1") != NULL;

		keep_alive = is_http_11;
		if (connection != NULL) {
			if (is_http_11) {
				keep_alive = strcasestr(connection, "close") == NULL;
			} else {
				keep_alive = strcasestr(connection, "keep-alive") != NULL;
			}
		}

		client->http.flags.http_11 = is_http_11;
		client->http.flags.keep_alive = keep_alive;

		if (key != NULL) {
			_do_websocket_upgrade(client, &headers, connection, key);
			*used = rbuff->len;
			status = PROT_OK;
		} else {
			status = _do_http(client, &headers, head_start);
			*used += body_start - (rbuff->str + *used);
		}
	}

	if (client->protocol == protocol_http && status == PROT_OK) {
		if (rbuff->len < (client->http.body_len + *used)) {
			status = PROT_AGAIN;
		} else {
			gchar *start = rbuff->str + *used;
			gchar replaced = start[client->http.body_len];

			start[client->http.body_len] = '\0';
			_handle_body(client, start);
			start[client->http.body_len] = replaced;

			*used += client->http.body_len;
			client->http.body_len = 0;
		}
	}

	return status;
}

void protocol_http_heartbeat(
	struct client *client G_GNUC_UNUSED,
	const struct protocol_heartbeat *hb G_GNUC_UNUSED)
{

}

struct protocol_frames protocol_http_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json)
{
	GString *raw;
	GString *def;

	raw = protocol_raw_format(ev_path, ev_extra, server_cb, json);
	g_string_append_c(raw, '\n');
	def = _build_response(STATUS_200, raw);

	return (struct protocol_frames){
		.def = def,
		.raw = raw,
	};
}

void protocol_http_send(
	struct client *surrogate,
	const struct protocol_frames *pframes)
{
	qev_lock(surrogate);

	// @todo remove this after testing
	ASSERT(qev_is_surrogate(surrogate), "Sending message to client that isn't surrogate");

	if (surrogate->http.flags.incoming || surrogate->http.client == NULL) {
		qev_write(surrogate, pframes->raw->str, pframes->raw->len);
	} else {
		_send_response(surrogate->http.client, pframes->def);
		surrogate->http.client = NULL;
	}

	qev_unlock(surrogate);
}

void protocol_http_close(struct client *client, guint reason)
{
	if (qev_is_surrogate(client)) {
		struct client *surrogate = client;
		struct client_table *tbl = _clients + surrogate->http.tbl;

		qev_lock_write_lock(&tbl->lock);

		// During shutdown, the hash tables are freed before clients; it's
		// not a big deal as no important references are maintained in them
		if (tbl->tbl != NULL) {
			INFO("Removing surrogate");
			g_hash_table_remove(tbl->tbl, &client->http.sid);
		}

		client = surrogate->http.client;
		if (client == NULL) {
			return;
		}

		reason = HTTP_NO_SURROGATE;
	}

	switch (reason) {
		case HTTP_BAD_REQUEST:
			_send(client, STATUS_400, NULL);
			break;

		case HTTP_NO_SURROGATE:
			_send(client, STATUS_403, NULL);
			break;

		case HTTP_LENGTH_REQUIRED:
			_send(client, STATUS_411, NULL);
			break;

		case QEV_CLOSE_READ_HIGH:
			_send(client, STATUS_413, NULL);
			break;

		default:
			client->http.client = NULL;
			break;
	}
}
