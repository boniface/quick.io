/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

/**
 * From: http://tools.ietf.org/html/rfc6455#section-1.3
 */
#define HASH_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

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

/**
 * Completely disables caching for HTTP requests
 */
#define HTTP_NOCACHE \
	"Cache-Control: no-cache, no-store, must-revalidate\r\n" \
	"Pragma: no-cache\r\n" \
	"Expires: 0\r\n"

#define HTTP_COMMON \
	HTTP_NOCACHE \
	"Connection: Keep-Alive\r\n" \
	"Keep-Alive: timeout=60\r\n"

/**
 * Ready for the B64-encoded key to be appended. Must be followed with \r\n\r\n
 */
#define HTTP_101 \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	HTTP_NOCACHE \
	"Sec-WebSocket-Accept: "

#define HTTP_200 \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_200_BODY \
	"HTTP/1.1 200 OK\r\n" \
	HTTP_COMMON \
	"Content-Length: "

#define HTTP_400 \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_403 \
	"HTTP/1.1 403 Forbidden\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_405 \
	"HTTP/1.1 405 Method Not Allowed\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_411 \
	"HTTP/1.1 411 Length Required\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_426 \
	"HTTP/1.1 426 Upgrade Required\r\n" \
	HTTP_COMMON "\r\n"

#define HTTP_OPTIONS \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length: 0\r\n" \
	HTTP_COMMON "\r\n"

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

static struct client* _get_client(gchar *head_start)
{
	union {
		uuid_t u;
		__uint128_t i;
	} uuid;
	struct client *client;
	guint which;
	gchar *sid = strstr(head_start, "sid=");

	if (sid == NULL) {
		return NULL;
	}

	sid += 4;
	strtok(sid, " ");
	if (uuid_parse(sid, uuid.u) != 0) {
		return NULL;
	}

	which = uuid.i % G_N_ELEMENTS(_clients);

	qev_lock_read_lock(&_clients[which].lock);
	client = g_hash_table_lookup(_clients[which].tbl, sid);
	qev_lock_read_unlock(&_clients[which].lock);

	if (client == NULL) {
		qev_lock_write_lock(&_clients[which].lock);

		client = g_hash_table_lookup(_clients[which].tbl, sid);
		if (client == NULL) {
			client = protocols_new_surrogate(protocol_http);
			client->http.sid = g_strdup(sid);
			client->http.tbl = which;
			g_hash_table_insert(_clients[which].tbl, client->http.sid, client);
		}

		qev_lock_write_unlock(&_clients[which].lock);
	}

	return client;
}

static void _handle_post(struct client *client, gchar *start)
{
	gchar *saveptr = NULL;

	if (client->http.client == NULL) {
		/*
		 * No surrogate was found, and the body has arrived, so just ignore
		 * everything (since there's nothing to be done with it), and send
		 * back an error.
		 */
		qev_write(client, HTTP_403, strlen(HTTP_403));
	} else {
		while (TRUE) {
			gchar *msg = strtok_r(start, "\n", &saveptr);
			start = NULL;
			if (msg == NULL) {
				break;
			}

			protocol_raw_handle(client->http.client, msg);
		}

		qev_write(client, HTTP_200, strlen(HTTP_200));
	}
}

static void _do_websocket_upgrade(
	struct client *client,
	const struct protocol_headers *headers,
	const gchar *key)
{
	gchar *connection = protocol_util_headers_get(headers, HTTP_CONNECTION);
	gchar *protocol = protocol_util_headers_get(headers, HTTP_PROTOCOL_KEY);
	gchar *upgrade = protocol_util_headers_get(headers, HTTP_UPGRADE);
	gchar *version = protocol_util_headers_get(headers, HTTP_VERSION_KEY);

	if (upgrade == NULL ||
		connection == NULL ||
		strstr(connection, "Upgrade") == NULL) {

		qev_stats_counter_inc(_stat_handshakes_http_missing_upgrade);
		qev_write(client, HTTP_426, strlen(HTTP_426));
	} else if (key == NULL ||
		g_strcmp0(protocol, "quickio") != 0 ||
		g_strcmp0(version, "13") != 0) {

		qev_stats_counter_inc(_stat_handshakes_http_invalid);
		qev_write(client, HTTP_400, strlen(HTTP_400));
	} else {
		gsize b64len;
		gint state = 0;
		gint save = 0;
		GString *buff = qev_buffer_get();
		GString *b64 = qev_buffer_get();

		qev_buffer_append(buff, key);
		qev_buffer_append_len(buff, HASH_KEY, strlen(HASH_KEY));

		SHA1((guchar*)buff->str, buff->len, (guchar*)buff->str);
		g_string_set_size(buff, SHA_DIGEST_LENGTH);

		/*
		 * This line brought to your courtesy of "wut". Wut, for when you know
		 * you have to do something but just take their word for it.
		 * See: https://developer.gnome.org/glib/2.37/glib-Base64-Encoding.html#g-base64-encode-step
		 */
		qev_buffer_ensure(b64, (SHA_DIGEST_LENGTH / 3 + 1) * 4 + 4);

		b64len = g_base64_encode_step((guchar*)buff->str, SHA_DIGEST_LENGTH,
						FALSE, b64->str, &state, &save);
		b64len += g_base64_encode_close(FALSE, b64->str + b64len, &state, &save);
		g_string_set_size(b64, b64len);

		qev_buffer_clear(buff);
		qev_buffer_append_len(buff, HTTP_101, strlen(HTTP_101));
		qev_buffer_append_buff(buff, b64);
		qev_buffer_append_len(buff, "\r\n\r\n", 4);
		qev_write(client, buff->str, buff->len);

		qev_buffer_put(buff);
		qev_buffer_put(b64);

		protocols_switch(client, protocol_rfc6455);
	}
}

static GString* _http_200_body(GString *msgs)
{
	GString *buff = qev_buffer_get();

	qev_buffer_append_len(buff, HTTP_200_BODY, strlen(HTTP_200_BODY));
	qev_buffer_append_uint(buff, msgs->len);
	qev_buffer_append_len(buff, "\r\n\r\n", 4);
	qev_buffer_append_buff(buff, msgs);

	return buff;
}

static enum protocol_status _do_http(
	struct client *client,
	const struct protocol_headers *headers,
	gchar *head_start)
{
	guint body_len;
	gchar *len_end;
	gchar *content_len;

	content_len = protocol_util_headers_get(headers, HTTP_CONTENT_LENGTH);
	if (content_len == NULL) {
		qev_write(client, HTTP_411, strlen(HTTP_411));
		return PROT_FATAL;
	}

	body_len = (guint)g_ascii_strtoull(content_len, &len_end, 10);
	if (body_len == 0 && len_end == content_len) {
		qev_write(client, HTTP_400, strlen(HTTP_400));
		return PROT_FATAL;
	}

	client->http.body_len = body_len;

	if (g_str_has_prefix(head_start, "POST")) {
		struct client *surrogate = _get_client(head_start);

		if (g_str_has_prefix(head_start + 4, " /poll")) {
			GString *msgs;

			qev_lock(surrogate);

			if (surrogate->http.client != NULL) {
				qev_write(surrogate->http.client, HTTP_200, strlen(HTTP_200));
				qev_unref(surrogate->http.client);
				surrogate->http.client = NULL;
			}

			msgs = qev_surrogate_flush(surrogate);
			if (msgs != NULL) {
				GString *buff = _http_200_body(msgs);

				qev_write(client, buff->str, buff->len);

				qev_buffer_put(buff);
				qev_buffer_put(msgs);
			} else {
				surrogate->http.client = qev_ref(client);
			}

			qev_unlock(surrogate);
		}

		client->http.client = qev_ref(surrogate);
	} else {
		qev_write(client, HTTP_405, strlen(HTTP_405));
	}

	return PROT_OK;
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
		_clients[i].tbl = g_hash_table_new(g_str_hash, g_str_equal);
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
		g_str_has_prefix(rbuff->str, "POST /")) {
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
		gchar *head_start = rbuff->str + *used;
		gchar *body_start = NULL;

		body_start = _find_header_end(head_start);
		if (body_start == NULL) {
			return PROT_AGAIN;
		}

		protocol_util_headers(head_start, &headers);
		key = protocol_util_headers_get(&headers, HTTP_CHALLENGE_KEY);

		if (key != NULL) {
			_do_websocket_upgrade(client, &headers, key);
			*used = rbuff->len;
			status = PROT_OK;
		} else {
			status = _do_http(client, &headers, head_start);
			*used += body_start - (rbuff->str + *used);
		}
	}

	if (client->protocol == protocol_http &&
		status == PROT_OK &&
		client->http.body_len > 0) {

		if (rbuff->len < (client->http.body_len + *used)) {
			status = PROT_AGAIN;
		} else {
			gchar *start = rbuff->str + *used;
			gchar replaced = start[client->http.body_len];

			start[client->http.body_len] = '\0';
			_handle_post(client, start);
			start[client->http.body_len] = replaced;

			*used += rbuff->len - *used;

			qev_unref(client->http.client);
			client->http.client = NULL;
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
	def = _http_200_body(raw);

	return (struct protocol_frames){
		.def = def,
		.raw = raw,
	};
}

void protocol_http_send(
	struct client *client,
	const struct protocol_frames *pframes)
{
	qev_lock(client);

	// @todo remove this after testing
	ASSERT(qev_is_surrogate(client), "Sending message to client that isn't surrogate");

	if (client->http.client != NULL) {
		qev_write(client->http.client, pframes->def->str, pframes->def->len);

		qev_unref(client->http.client);
		client->http.client = NULL;
	} else {
		qev_write(client, pframes->raw->str, pframes->raw->len);
	}

	qev_unlock(client);
}

void protocol_http_close(struct client *client, guint reason G_GNUC_UNUSED)
{
	qev_unref(client->http.client);
	client->http.client = NULL;

	g_free(client->http.sid);
	client->http.sid = NULL;
}
