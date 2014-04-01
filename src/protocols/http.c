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

#define HTTP_RESPONSE \
	"HTTP/1.0 %s\r\n" \
	HTTP_COMMON \
	"Content-Length: "

#define HTTP_HEADER_TERMINATOR "\r\n\r\n"
#define HTTP_HEADER_TERMINATOR2 "\n\n"

enum status {
	STATUS_200,
	STATUS_400,
	STATUS_403,
	STATUS_405,
	STATUS_411,
	STATUS_413,
	STATUS_426,
};

struct client_table {
	GHashTable *tbl;
	qev_lock_t lock;
};

static gchar *_status_lines[] = {
	"200 OK",
	"400 Bad Request",
	"403 Forbidden",
	"405 Method Not Allowed",
	"411 Length Required",
	"413 Request Entity Too Large",
	"426 Upgrade Required",
};

static qev_stats_counter_t *_stat_handshakes_http;
static qev_stats_counter_t *_stat_handshakes_http_invalid;
static qev_stats_counter_t *_stat_handshakes_http_missing_upgrade;

static GString *_status_responses[G_N_ELEMENTS(_status_lines)];

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
		strstr(rbuff->str, HTTP_HEADER_TERMINATOR2) != NULL;
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

static GString* _build_response(enum status status, const GString *body)
{
	GString *buff = qev_buffer_get();
	g_string_printf(buff, HTTP_RESPONSE, _status_lines[status]);

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

static struct client* _steal_client(struct client *c)
{
	struct client *ret;

	qev_lock(c);

	ret = c->http.client;
	c->http.client = NULL;

	qev_unlock(c);

	return ret;
}

static void _surr_release_client(
	struct client *surrogate,
	struct client *client)
{
	gboolean unref = FALSE;

	if (surrogate == NULL) {
		return;
	}

	qev_lock(surrogate);

	if (surrogate->http.client == client) {
		unref = TRUE;
		surrogate->http.client = NULL;
	}

	qev_unlock(surrogate);

	if (unref) {
		qev_unref(client);
	}
}

/**
 * Sends a full HTTP response to the client.
 *
 * @param client
 *     The client to get a message. Not a surrogate.
 * @param resp
 *     The full HTTP response to send
 *
 * @return
 *     If the response was successfully sent. Might fail if another thread
 *     already sent a response out on this client.
 */
static gboolean _send_response(struct client *client, GString *resp)
{
	struct client *surrogate = NULL;
	gboolean sent = FALSE;

	if (client == NULL) {
		goto out;
	}

	qev_lock(client);

	surrogate = _steal_client(client);

	if (client->http.flags.in_request) {
		client->http.flags.in_request = FALSE;

		qev_write(client, resp->str, resp->len);
		client->last_send = qev_monotonic;

		if (surrogate != NULL) {
			surrogate->last_send = qev_monotonic;
		}

		if (!client->http.flags.keep_alive) {
			qev_close(client, HTTP_DONE);
		}

		sent = TRUE;
	}

	qev_unlock(client);

	_surr_release_client(surrogate, client);
	qev_unref(surrogate);

out:
	return sent;
}

/**
 * Sends a pre-prepared HTTP response with length 0.
 */
static gboolean _send_error(struct client *client, enum status status) {
	return _send_response(client, _status_responses[status]);
}

static void _surr_replace(struct client *surrogate, struct client *new_poller)
{
	/*
	 * For pipelined requests, old_poller will never be new_poller:
	 * the request is responded to, and the poller released, before replacement
	 * is ever attempted. So no need to worry about that case here.
	 */

	struct client *old_poller;

	qev_lock(surrogate);
	qev_lock(new_poller);

	old_poller = surrogate->http.client;

	if (qev_is_closing(new_poller)) {
		surrogate->http.client = NULL;
	} else {
		surrogate->http.client = qev_ref(new_poller);
	}

	qev_unlock(new_poller);
	qev_unlock(surrogate);

	if (old_poller != NULL) {
		_send_error(old_poller, STATUS_200);
		qev_unref(old_poller);
	}
}

static void _surr_send(
	struct client *surrogate,
	struct client *new_poller,
	const GString *msgs)
{
	gboolean sent = FALSE;
	GString *resp = _build_response(STATUS_200, msgs);
	struct client *poller =  _steal_client(surrogate);

	if (poller != NULL) {
		sent = _send_response(poller, resp);
		qev_unref(poller);
	}

	if (sent) {
		_surr_replace(surrogate, new_poller);
	} else {
		// @todo remove ASSERT after all testing is complete -- maybe just turn it into a critical that closes surrogate and client and logs it?
		ASSERT(_send_response(new_poller, resp),
			"Failed to send response on new_poller");
	}

	qev_buffer_put(resp);
}

static void _surr_route(
	struct client *surrogate,
	struct client *from,
	gchar *body)
{
	GString *msgs;
	gchar *saveptr = NULL;

	surrogate->http.flags.incoming = TRUE;

	while (TRUE) {
		gchar *msg = strtok_r(body, "\n", &saveptr);
		body = NULL;
		if (msg == NULL) {
			break;
		}

		if (protocol_raw_handle(surrogate, msg) != PROT_OK) {
			_send_error(from, STATUS_400);
			return;
		}
	}

	qev_lock(surrogate);

	surrogate->http.flags.incoming = FALSE;

	msgs = qev_surrogate_flush(surrogate);
	if (msgs == NULL) {
		_surr_replace(surrogate, from);
	} else {
		_surr_send(surrogate, from, msgs);
	}

	qev_unlock(surrogate);

	qev_buffer_put(msgs);
}

static struct client* _surr_find(
	const __uint128_t sid,
	const gboolean or_create)
{
	struct client *surrogate;
	guint which = sid % G_N_ELEMENTS(_clients);
	struct client_table *tbl = _clients + which;

	qev_lock_read_lock(&tbl->lock);
	surrogate = g_hash_table_lookup(tbl->tbl, &sid);
	qev_lock_read_unlock(&tbl->lock);

	if (surrogate == NULL && or_create) {
		qev_lock_write_lock(&tbl->lock);

		surrogate = g_hash_table_lookup(tbl->tbl, &sid);
		if (surrogate == NULL) {
			surrogate = protocols_new_surrogate(protocol_http);
			if (surrogate != NULL) {
				surrogate->last_send = qev_monotonic;
				surrogate->http.sid = sid;
				surrogate->http.tbl = which;
				g_hash_table_insert(tbl->tbl, &surrogate->http.sid, surrogate);
			}
		}

		qev_lock_write_unlock(&tbl->lock);
	}

	return surrogate;
}

static struct client* _get_surrogate(gchar *url)
{
	union {
		uuid_t u;
		__uint128_t i;
	} uuid;
	gboolean new_client;
	gchar *sid = strstr(url, "sid=");

	// @todo test case for /connect=true/?sid= <- even worth caring about?

	if (sid == NULL) {
		return NULL;
	}

	new_client = strstr(url, "connect=true") != NULL;
	sid += 4;
	strtok(sid, " &");

	if (uuid_parse(sid, uuid.u) != 0) {
		return NULL;
	}

	return _surr_find(uuid.i, new_client);
}

static void _do_body(struct client *client, gchar *start)
{
	if (client->http.flags.iframe_requested) {

	} else if (!client->http.flags.is_post) {
		_send_error(client, STATUS_405);
	} else {
		/*
		 * Safe to access without a lock: at this point, only this thread
		 * will have a reference to the client, so it's safe to go and molest
		 * the surrogate.
		 */
		struct client *surrogate = client->http.client;

		if (surrogate == NULL) {
			/*
			 * No surrogate was found, and the body has arrived, so just ignore
			 * everything (since there's nothing to be done with it), and send
			 * back an error.
			 */
			_send_error(client, STATUS_403);
		} else {
			_surr_route(surrogate, client, start);
		}
	}
}

static void _do_headers_websocket(
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
		_send_error(client, STATUS_426);
	} else if (key == NULL ||
		g_strcmp0(protocol, "quickio") != 0 ||
		g_strcmp0(version, "13") != 0) {

		qev_stats_counter_inc(_stat_handshakes_http_invalid);
		_send_error(client, STATUS_400);
	} else {
		protocol_rfc6455_upgrade(client, key);
	}
}

static enum protocol_status _do_headers_http(
	struct client *client,
	const struct protocol_headers *headers,
	gchar *head_start)
{
	gchar *content_len;
	struct client *surrogate;
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

	surrogate = _get_surrogate(url);

	if (surrogate != NULL) {
		qev_lock(surrogate);
		qev_lock(client);

		if (!qev_is_closing(surrogate) && !qev_is_closing(client)) {
			client->http.client = qev_ref(surrogate);
		}

		qev_unlock(client);
		qev_unlock(surrogate);
	}

	client->http.body_len = body_len;
	client->http.flags.is_post = is_post;
	client->http.flags.iframe_requested = g_str_has_prefix(url, "/iframe");

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

	for (i = 0; i < G_N_ELEMENTS(_status_responses); i++) {
		qev_buffer_put0(&_status_responses[i]);
	}
}

void protocol_http_init()
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(_clients); i++) {
		_clients[i].tbl = g_hash_table_new(_uint128_hash, _uint128_equal);
	}

	for (i = 0; i < G_N_ELEMENTS(_status_responses); i++) {
		_status_responses[i] = _build_response(i, NULL);
	}

	qev_cleanup_fn_full(_cleanup, TRUE);

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
	enum protocol_status status = PROT_OK;
	GString *rbuff = client->qev_client.rbuff;

	if (client->http.body_len == 0) {
		gchar *key;
		gchar *connection;
		gboolean is_http_11;
		gboolean keep_alive;
		struct protocol_headers headers;
		gchar *head_start = rbuff->str + *used;
		gchar *body_start = NULL;

		body_start = _find_header_end(head_start);
		if (body_start == NULL) {
			return PROT_AGAIN;
		}

		_send_error(client, STATUS_200);

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

		client->http.flags.in_request = TRUE;
		client->http.flags.keep_alive = keep_alive;

		if (key != NULL) {
			_do_headers_websocket(client, &headers, connection, key);
			*used = rbuff->len;
			status = PROT_OK;
		} else {
			status = _do_headers_http(client, &headers, head_start);
			*used += body_start - (rbuff->str + *used);
		}
	}

	if (client->protocol.prot == protocol_http && status == PROT_OK) {
		if (rbuff->len < (client->http.body_len + *used)) {
			status = PROT_AGAIN;
		} else {
			gchar *start = rbuff->str + *used;
			gchar replaced = start[client->http.body_len];

			start[client->http.body_len] = '\0';
			_do_body(client, start);
			start[client->http.body_len] = replaced;

			*used += client->http.body_len;
			client->http.body_len = 0;
		}
	}

	return status;
}

void protocol_http_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb)
{
	if (qev_is_surrogate(client)) {
		struct client *surrogate = client;

		if (surrogate->http.client == NULL &&
			surrogate->last_send < hb->timeout) {

			qev_close(surrogate, RAW_HEARTATTACK);
		}
	} else {
		if (client->http.client == NULL) {
			if (client->last_send < hb->heartbeat) {
				qev_close(client, RAW_HEARTATTACK);
			}
		} else {
			if (client->last_send < hb->poll) {
				_send_error(client, STATUS_200);
			}
		}
	}
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
	gboolean sent = FALSE;

	qev_lock(surrogate);

	if (!surrogate->http.flags.incoming) {
		struct client *client = _steal_client(surrogate);
		sent = _send_response(client, pframes->def);
		qev_unref(client);
	}

	if (!sent) {
		qev_write(surrogate, pframes->raw->str, pframes->raw->len);
	}

	qev_unlock(surrogate);
}

void protocol_http_close(struct client *client, guint reason)
{
	if (qev_is_surrogate(client)) {
		struct client *surrogate = client;
		struct client_table *tbl = _clients + surrogate->http.tbl;

		qev_lock_write_lock(&tbl->lock);

		g_hash_table_remove(tbl->tbl, &client->http.sid);

		qev_lock_write_unlock(&tbl->lock);

		client = _steal_client(surrogate);
		_send_error(client, STATUS_403);
		qev_unref(client);
	} else {
		struct client *surrogate = _steal_client(client);

		switch (reason) {
			case HTTP_BAD_REQUEST:
				_send_error(client, STATUS_400);
				break;

			case HTTP_LENGTH_REQUIRED:
				_send_error(client, STATUS_411);
				break;

			case QEV_CLOSE_READ_HIGH:
				_send_error(client, STATUS_413);
				break;
		}

		if (surrogate != NULL) {
			qev_close(surrogate, HTTP_DONE);
			qev_unref(surrogate);
		}
	}
}
