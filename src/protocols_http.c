/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
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
	STATUS_501,
};

struct client_table {
	GHashTable *tbl;
	qev_lock_t lock;
};

struct _http_status {
	guint code;
	gchar *line;
};

static struct _http_status _statuses[] = {
	{	.code = 200,
		.line = "200 OK\r\n",
	},
	{	.code = 400,
		.line = "400 Bad Request\r\n",
	},
	{	.code = 403,
		.line = "403 Forbidden\r\n",
	},
	{	.code = 405,
		.line = "405 Method Not Allowed\r\n",
	},
	{	.code = 411,
		.line = "411 Length Required\r\n",
	},
	{	.code = 413,
		.line = "413 Request Entity Too Large\r\n",
	},
	{	.code = 426,
		.line = "426 Upgrade Required\r\n",
	},
	{	.code = 501,
		.line = "501 Not Implemented\r\n",
	},
};

#include "protocols_http_html_iframe.c"
#include "protocols_http_html_error.c"

static qev_stats_counter_t *_stat_requests_iframe;
static qev_stats_counter_t *_stat_requests_invalid_method;
static qev_stats_counter_t *_stat_requests_poll;
static qev_stats_counter_t *_stat_requests_immediate_response;
static qev_stats_counter_t *_stat_responses[G_N_ELEMENTS(_statuses)];
static qev_stats_counter_t *_stat_headers_upgrade_invalid;
static qev_stats_counter_t *_stat_headers_upgrade_missing;
static qev_stats_counter_t *_stat_surrogates_opened;
static qev_stats_counter_t *_stat_surrogates_closed;
static qev_stats_timer_t *_stat_route_time;

static GString *_iframe_source;
static GString *_status_responses[G_N_ELEMENTS(_statuses)];

static struct client_table _clients[64];

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

static GString* _build_response_full(
	const enum status status,
	const GString *body,
	const gboolean is_iframe)
{
	GString *buff = qev_buffer_get();
	guint64 len = body != NULL ? body->len : 0;

	g_string_append(buff, "HTTP/1.0 ");
	g_string_append(buff, _statuses[status].line);
	g_string_append(buff,
		HTTP_COMMON
		"Content-Length: ");

	qev_buffer_append_uint(buff, len);

	if (is_iframe) {
		g_string_append(buff, "\r\nContent-Type: text/html\r\n\r\n");
	} else {
		g_string_append(buff, "\r\nContent-Type: text/plain\r\n\r\n");
	}

	if (body != NULL) {
		qev_buffer_append_buff(buff, body);
	}

	return buff;
}

static GString* _build_response(const enum status status, const GString *body)
{
	return _build_response_full(status, body, FALSE);
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
 * @param status
 *     The status sent, only used for stats
 * @param resp
 *     The full HTTP response to send
 *
 * @return
 *     If the response was successfully sent. Might fail if another thread
 *     already sent a response out on this client.
 */
static gboolean _send_response(
	struct client *client,
	enum status status,
	GString *resp)
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

	if (sent) {
		qev_stats_counter_inc(_stat_responses[status]);
	}

out:
	return sent;
}

/**
 * Sends a pre-prepared HTTP response with length 0.
 */
static gboolean _send_error(struct client *client, enum status status) {
	return _send_response(client, status, _status_responses[status]);
}

static void _surr_replace(struct client *surrogate, struct client *new_poller)
{
	/*
	 * For pipelined requests, old_poller will never be new_poller:
	 * the request is responded to, and the poller released, before replacement
	 * is ever attempted. So no need to worry about that case here.
	 */

	gboolean closing;
	struct client *old_poller;

	qev_lock(surrogate);
	qev_lock(new_poller);

	old_poller = surrogate->http.client;
	surrogate->http.client = NULL;

	closing = qev_is_closing(surrogate) || qev_is_closing(new_poller);
	if (!closing) {
		surrogate->http.client = qev_ref(new_poller);
	}

	qev_unlock(new_poller);
	qev_unlock(surrogate);

	if (closing) {
		_send_error(new_poller, STATUS_403);
	}

	if (old_poller != NULL) {
		_send_error(old_poller, closing ? STATUS_403 : STATUS_200);
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
		sent = _send_response(poller, STATUS_200, resp);
		qev_unref(poller);
	}

	if (sent) {
		_surr_replace(surrogate, new_poller);
	} else {
		// @todo remove ASSERT after all testing is complete -- maybe just turn it into a critical that closes surrogate and client and logs it?
		ASSERT(_send_response(new_poller, STATUS_200, resp),
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
		qev_stats_counter_inc(_stat_requests_poll);
	} else {
		_surr_send(surrogate, from, msgs);
		qev_stats_counter_inc(_stat_requests_immediate_response);
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
				qev_stats_counter_inc(_stat_surrogates_opened);
			}
		}

		qev_lock_write_unlock(&tbl->lock);
	}

	return surrogate;
}

static struct client* _get_surrogate(gchar *url)
{
	guint i;
	gboolean new_client;
	__uint128_t uuid = 0;
	gchar *sid = strstr(url, "sid=");

	// @todo test case for /connect=true/?sid= <- even worth caring about?

	if (sid == NULL) {
		return NULL;
	}

	new_client = strstr(url, "connect=true") != NULL;
	sid += 4;
	strtok(sid, " &");

	/*
	 * 0) Each character is 4 bits of data (0-15, 0x0-0xf)
	 * 1) If it's not 32 bytes long, that's automatically an error
	 * 2) Each character is interpreted directly into the 128bit number
	 *    as its 4 bits
	 */
	for (i = 0; i < 32 && g_ascii_isxdigit(sid[i]); i++) {
		__uint128_t n = g_ascii_xdigit_value(sid[i]);
		uuid |= n << (i * 4);
	}

	if (i != 32) {
		return NULL;
	}

	return _surr_find(uuid, new_client);
}

static void _do_body(struct client *client, gchar *start)
{
	if (cfg_public_address == NULL) {
		_send_error(client, STATUS_501);
	} else if (client->http.flags.iframe_requested) {
		_send_response(client, STATUS_200, _iframe_source);
		qev_stats_counter_inc(_stat_requests_iframe);
	} else if (!client->http.flags.is_post) {
		_send_error(client, STATUS_405);
		qev_stats_counter_inc(_stat_requests_invalid_method);
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

static enum protocol_status _do_headers_websocket(
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

		qev_stats_counter_inc(_stat_headers_upgrade_missing);
		_send_error(client, STATUS_426);
	} else if (key == NULL ||
		g_strcmp0(protocol, "quickio") != 0 ||
		g_strcmp0(version, "13") != 0) {

		qev_stats_counter_inc(_stat_headers_upgrade_invalid);
		_send_error(client, STATUS_400);
	} else {
		protocol_rfc6455_upgrade(client, key);
		return PROT_AGAIN;
	}

	return PROT_OK;
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

	qev_buffer_put0(&_iframe_source);
}

void protocol_http_init()
{
	guint i;
	GString *buff = qev_buffer_get();

	for (i = 0; i < G_N_ELEMENTS(_clients); i++) {
		_clients[i].tbl = g_hash_table_new(_uint128_hash, _uint128_equal);
	}

	for (i = 0; i < G_N_ELEMENTS(_status_responses); i++) {
		gchar desc[256];

		g_string_printf(buff, "responses.%u", _statuses[i].code);
		snprintf(desc, sizeof(desc), "The number of HTTP %us sent", _statuses[i].code);

		_stat_responses[i] = qev_stats_counter(
			"protocol.http", buff->str, TRUE,
			desc);
		qev_buffer_clear(buff);

		_status_responses[i] = _build_response(i, NULL);
	}
	qev_cleanup_fn_full(_cleanup, TRUE);

	qev_buffer_put0(&_status_responses[STATUS_501]);
	g_string_append_len(buff,
		(char*)src_protocols_http_html_error_c_html,
		src_protocols_http_html_error_c_html_len);
	_status_responses[STATUS_501] = _build_response(STATUS_501, buff);
	qev_buffer_clear(buff);

	if (cfg_public_address == NULL) {
		WARN("HTTP provider not running: `public-address` must be set "
			"to be able to run.");
	} else {
		g_string_append_len(buff,
			(char*)src_protocols_http_html_iframe_c_html,
			src_protocols_http_html_iframe_c_html_len);
		qev_buffer_replace_str(buff, "{PUBLIC_ADDRESS}", cfg_public_address);
		_iframe_source = _build_response_full(STATUS_200, buff, TRUE);
		qev_buffer_clear(buff);
	}

	_stat_requests_iframe = qev_stats_counter(
		"protocol.http","requests.iframe", TRUE,
		"Number of GET requests for /iframe");
	_stat_requests_invalid_method = qev_stats_counter(
		"protocol.http","requests.invalid_method", TRUE,
		"Number of requests sent with an invalid method");
	_stat_requests_poll = qev_stats_counter(
		"protocol.http","requests.poll", TRUE,
		"Number of polling requests that were received and put into a polling state");
	_stat_requests_immediate_response = qev_stats_counter(
		"protocol.http","requests.immediate_response", TRUE,
		"Number of polling requests that were received and responded to immediately");
	_stat_headers_upgrade_invalid = qev_stats_counter(
		"protocol.http","upgrade.invalid", TRUE,
		"HTTP upgrade requests with an inappropriate version or protocol");
	_stat_headers_upgrade_missing = qev_stats_counter(
		"protocol.http","upgrade.missing", TRUE,
		"Requests that look like a WebSocket upgrade but that were missing the"
		" Upgrade header");
	_stat_surrogates_opened = qev_stats_counter(
		"protocol.http","surrogates.opened", TRUE,
		"How many surrogates were created in response to requests");
	_stat_surrogates_closed = qev_stats_counter(
		"protocol.http","surrogates.closed", TRUE,
		"How many surrogates were closed");
	_stat_route_time = qev_stats_timer(
		"protocol.http", "route",
		"How long it took to route a single event, after parsing HTTP headers");

	qev_buffer_put(buff);
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

	// Proxies reset their timers based on data going in either direction
	client->last_send = qev_monotonic;

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
			status = _do_headers_websocket(client, &headers, connection, key);
			*used = rbuff->len;
		} else {
			status = _do_headers_http(client, &headers, head_start);
			*used += body_start - (rbuff->str + *used);
		}
	}

	if (client->protocol.prot == protocol_http && status == PROT_OK) {
		guint64 len;

		if (!qev_safe_uadd(client->http.body_len, *used, &len)) {
			status = PROT_FATAL;
		} else if (rbuff->len < len) {
			status = PROT_AGAIN;
		} else {
			gchar *start = rbuff->str + *used;
			gchar replaced = start[client->http.body_len];

			start[client->http.body_len] = '\0';
			qev_stats_time(_stat_route_time, {
				_do_body(client, start);
			});
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
		sent = _send_response(client, STATUS_200, pframes->def);
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

		qev_stats_counter_inc(_stat_surrogates_closed);
	} else {
		struct client *surrogate = _steal_client(client);

		switch (reason) {
			case HTTP_BAD_REQUEST:
				_send_error(client, STATUS_400);
				break;

			case HTTP_LENGTH_REQUIRED:
				_send_error(client, STATUS_411);
				break;

			case QEV_CLOSE_OUT_OF_MEM:
				_send_error(client, STATUS_413);
				break;
		}

		if (surrogate != NULL) {
			qev_close(surrogate, HTTP_DONE);
			qev_unref(surrogate);
		}
	}
}
