/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
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
 * Completely disables caching for HTTP requests
 */
#define HTTP_NOCACHE \
	"Cache-Control: no-cache, no-store, must-revalidate\r\n" \
	"Pragma: no-cache\r\n" \
	"Expires: 0\r\n"

#define HTTP_400 \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Connection: close\r\n" \
	HTTP_NOCACHE "\r\n"

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

#define HTTP_HEADER_TERMINATOR "\n\n"
#define WEB_SOCKET_HEADER_TERMINATOR "\r\n\r\n"

/**
 * Predefined and formatted RFC6455 messages
 */
#define QIO_HANDSHAKE "\x81\x09/qio/ohai"
#define HEARTBEAT "\x81""\x15""/qio/heartbeat:0=null"

/**
 * RFC6455 byte locations
 */
#define PAYLOAD_SHORT 125
#define PAYLOAD_MEDIUM 126
#define PAYLOAD_LONG 127
#define MASK_FIN 0x80
#define MASK_OPCODE 0x0f
#define MASKED_BIT 0x80
#define MASK_LEN 0x7f

#define OPCODE 0x0f
#define OPCODE_TEXT 0x01
#define OPCODE_CLOSE 0x08

/**
 * Protocol flags
 */
#define HTTP_HANDSHAKED 0x0001

/**
 * For all the parser callbacks, we need all this junk
 */
struct _parser_data {
	/**
	 * Socket buffer
	 */
	GString *buffer;

	/**
	 * The hash table to insert headers into
	 */
	GHashTable *headers;

	/**
	 * The key that was just found
	 */
	gchar *key;

	/**
	 * The length of the key that was just found
	 */
	gsize key_len;
};

static qev_stats_counter_t *_stat_handshakes_http;
static qev_stats_counter_t *_stat_handshakes_http_invalid;
static qev_stats_counter_t *_stat_handshakes_qio;
static qev_stats_counter_t *_stat_handshakes_qio_invalid;
static qev_stats_timer_t *_stat_route_time;

static enum protocol_status _decode(
	struct client *client,
	guint64 *len_,
	guint64 *frame_len)
{
	/*
	 * See http://tools.ietf.org/html/rfc6455#section-5.2 for more on what's
	 * going on here
	 */

	GString *rbuff = client->qev_client.rbuff;
	gchar *str = rbuff->str;
	gchar *msg = rbuff->str;
	guint16 header_len = 0;
	guint64 i;
	guint64 len;
	guint64 max;
	gchar mask[4];
	union {
		gchar c[sizeof(__uint128_t)];
		__uint128_t i;
	} mask128;

	if ((str[0] & OPCODE) == OPCODE_CLOSE) {
		qev_close(client, QEV_CLOSE_HUP);
		return PROT_FATAL;
	}

	if (rbuff->len < 6) {
		return PROT_AGAIN;
	}

	if ((str[0] & OPCODE) != OPCODE_TEXT) {
		qev_close(client, RFC6455_UNSUPPORTED_OPCODE);
		return PROT_FATAL;
	}

	if (!(str[1] & MASKED_BIT)) {
		qev_close(client, RFC6455_NO_MASK);
		return PROT_FATAL;
	}

	len = str[1] & MASK_LEN;

	if (len <= PAYLOAD_SHORT) {
		header_len = 6;
		memcpy(mask, str + 2, 4);
	} else if (len == PAYLOAD_MEDIUM) {
		header_len = 8;
		if (rbuff->len < header_len) {
			return PROT_AGAIN;
		}

		len = GUINT16_FROM_BE(*((guint16*)(str + 2)));
		memcpy(mask, str + 4, 4);
	} else {
		header_len = 14;
		if (rbuff->len < header_len) {
			return PROT_AGAIN;
		}

		len = GUINT64_FROM_BE(*((guint64*)(str + 2)));
		memcpy(mask, str + 10, 4);
	}

	if (rbuff->len < (header_len + len)) {
		return PROT_AGAIN;
	}


	*len_ = len;
	*frame_len = len + header_len;
	str += header_len;

	/*
	 * The following mess warrants an explanation: it's quite a bit
	 * faster than the naive decode.
	 *
	 * The following steps are taken:
	 *   0) Create a 128 bit masking key from 4 32bit ones.
	 *   1) Gob through 128 bits at a time, until the next XOR would go past
	 *      the end of the buffer.
	 *   2) Gob through 64 bits at a time, until the next XOR would overflow
	 *   3) Finish off going through any last characters.
	 */
	for (i = 0; i < sizeof(mask128) / sizeof(mask); i++) {
		memcpy(mask128.c + (i * sizeof(mask)), mask, sizeof(mask));
	}

	max = len - (len & (sizeof(__uint128_t) - 1));
	for (i = 0; i < max; i += sizeof(__uint128_t)) {
		__uint128_t from = *((__uint128_t*)(str + i));
		*((__uint128_t*)(msg + i)) = from ^ mask128.i;
	}

	max = len - (len & (sizeof(guint64) - 1));
	for (; i < max; i += sizeof(guint64)) {
		guint64 from = *((guint64*)(str + i));
		*((guint64*)(msg + i)) = from ^ mask128.i;
	}

	for (; i < len; i++) {
		msg[i] = str[i] ^ mask[i & 3];
	}

	*(msg + len) = '\0';

	if (!qio_str_is_utf8((guchar*)msg, len)) {
		qev_close(client, RFC6455_NOT_UTF8);
		return PROT_FATAL;
	}

	return PROT_OK;
}

static void _handshake_http(
	struct client *client,
	const gchar *key)
{
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

	b64len = g_base64_encode_step((guchar*)buff->str, SHA_DIGEST_LENGTH, FALSE, b64->str,
									&state, &save);
	b64len += g_base64_encode_close(FALSE, b64->str + b64len, &state, &save);
	g_string_set_size(b64, b64len);

	qev_buffer_clear(buff);
	qev_buffer_append_len(buff, HTTP_101, strlen(HTTP_101));
	qev_buffer_append_buff(buff, b64);
	qev_buffer_append_len(buff, "\r\n\r\n", 4);
	qev_write(client, buff->str, buff->len);

	qev_buffer_put(buff);
	qev_buffer_put(b64);

	g_string_truncate(client->qev_client.rbuff, 0);
}

static enum protocol_status _handshake_qio(struct client *client)
{
	gboolean good;
	guint64 len;
	guint64 frame_len;
	enum protocol_status status;

	status = _decode(client, &len, &frame_len);
	if (status != PROT_OK) {
		return status;
	}

	good = protocol_raw_check_handshake(client);
	if (good) {
		qev_stats_counter_inc(_stat_handshakes_qio);
		qev_write(client, QIO_HANDSHAKE, sizeof(QIO_HANDSHAKE) - 1);
		status = PROT_OK;
	} else {
		qev_close(client, QIO_CLOSE_INVALID_HANDSHAKE);
		status = PROT_FATAL;
	}

	return status;
}

void protocol_rfc6455_init()
{
	_stat_handshakes_http = qev_stats_counter("protocol.rfc6455",
								"handshakes.http", TRUE);
	_stat_handshakes_http_invalid = qev_stats_counter("protocol.rfc6455",
								"handshakes.http_invalid", TRUE);
	_stat_handshakes_qio = qev_stats_counter("protocol.rfc6455",
								"handshakes.qio", TRUE);
	_stat_handshakes_qio_invalid = qev_stats_counter("protocol.rfc6455",
								"handshakes.qio_invalid", TRUE);
	_stat_route_time = qev_stats_timer("protocol.rfc6455", "route");
}

enum protocol_handles protocol_rfc6455_handles(struct client *client)
{
	GString *rbuff = client->qev_client.rbuff;

	if (!g_str_has_suffix(rbuff->str, WEB_SOCKET_HEADER_TERMINATOR) &&
		!g_str_has_suffix(rbuff->str, HTTP_HEADER_TERMINATOR)) {
		return PROT_MAYBE;
	}

	if (!g_str_has_prefix(rbuff->str, "GET")) {
		return PROT_NO;
	}

	return PROT_YES;
}

enum protocol_status protocol_rfc6455_handshake(struct client *client)
{
	enum protocol_status status;

	if (!(client->protocol_flags & HTTP_HANDSHAKED)) {
		gchar *connection;
		gchar *key;
		gchar *protocol;
		gchar *upgrade;
		gchar *version;
		struct protocol_headers headers;

		protocol_util_headers(client->qev_client.rbuff, &headers);
		connection = protocol_util_headers_get(&headers, HTTP_CONNECTION);
		key = protocol_util_headers_get(&headers, HTTP_CHALLENGE_KEY);
		protocol = protocol_util_headers_get(&headers, HTTP_PROTOCOL_KEY);
		upgrade = protocol_util_headers_get(&headers, HTTP_UPGRADE);
		version = protocol_util_headers_get(&headers, HTTP_VERSION_KEY);

		if (upgrade == NULL ||
			connection == NULL ||
			key == NULL ||
			strstr(connection, "Upgrade") == NULL ||
			g_strcmp0(protocol, "quickio") != 0 ||
			g_strcmp0(version, "13") != 0) {

			/*
			 * At this point, the client is either invalid or a proxy that is
			 * refusing to speak proper WebSocket. If we close the proxy, it
			 * will just reconnect and continue speaking gibberish. If it's an
			 * invalid client, it really doesn't matter what we do anyway.
			 */

			qev_stats_counter_inc(_stat_handshakes_http_invalid);
			qev_close(client, QIO_CLOSE_NOT_SUPPORTED);
			status = PROT_FATAL;
		} else {
			_handshake_http(client, key);
			client->protocol_flags |= HTTP_HANDSHAKED;
			qev_stats_counter_inc(_stat_handshakes_http);
			status = PROT_AGAIN;
		}
	} else {
		status = _handshake_qio(client);
		if (status == PROT_FATAL) {
			qev_stats_counter_inc(_stat_handshakes_qio_invalid);
		}
	}

	return status;
}

enum protocol_status protocol_rfc6455_route(struct client *client)
{
	guint64 len;
	guint64 frame_len;
	enum protocol_status status;

	qev_stats_time(_stat_route_time, {
		status = _decode(client, &len, &frame_len);
		if (status == PROT_OK) {
			status = protocol_raw_handle(client, len, frame_len);
		}
	});

	return status;
}

void protocol_rfc6455_heartbeat(struct client *client, struct heartbeat *hb)
{
	protocol_raw_do_heartbeat(client, hb,
					HEARTBEAT, sizeof(HEARTBEAT) - 1);
}

GString* protocol_rfc6455_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json)
{
	GString *f = qev_buffer_get();
	GString *e = protocol_raw_format(ev_path, ev_extra, server_cb, json);
	const gsize len = e->len;

	g_string_append_c(f, '\x81');

	if (len <= PAYLOAD_SHORT) {
		g_string_append_c(f, (guint8)len);
	} else if (len <= 0xffff) {
		guint16 belen = GUINT16_TO_BE(len);
		g_string_append_c(f, (gchar)PAYLOAD_MEDIUM);
		qev_buffer_append_len(f, (gchar*)&belen, sizeof(belen));
	} else {
		guint64 belen = GUINT64_TO_BE(len);
		g_string_append_c(f, (gchar)PAYLOAD_LONG);
		qev_buffer_append_len(f, (gchar*)&belen, sizeof(belen));
	}

	qev_buffer_append_len(f, e->str, e->len);

	qev_buffer_put(e);

	return f;
}

void protocol_rfc6455_close(struct client *client, guint reason)
{
	if (client->protocol_flags & HTTP_HANDSHAKED) {
		switch (reason) {
			case QEV_CLOSE_EXITING:
				// error code: 1001
				qev_write(client, "\x88\x02\x03\xe9", 4);
				break;

			case QIO_CLOSE_INVALID_HANDSHAKE:
				// error code: 1002
				qev_write(client, "\x88\x13\x03\xea""invalid handshake", 21);
				break;

			case RAW_INVALID_EVENT_FORMAT:
				// error code: 1002
				qev_write(client, "\x88\x16\x03\xea""invalid event format", 24);
				break;

			case RFC6455_NO_MASK:
				// error code: 1002
				qev_write(client, "\x88\x17\x03\xea""client must mask data", 25);
				break;

			case RFC6455_UNSUPPORTED_OPCODE:
				// error code: 1003
				qev_write(client, "\x88\x02\x03\xeb", 4);
				break;

			case RFC6455_NOT_UTF8:
				// error code: 1007
				qev_write(client, "\x88\x02\x03\xef", 4);
				break;

			case QEV_CLOSE_TIMEOUT:
				qev_write(client, "\x88\x10\x03\xf0""client timeout", 18);
				break;

			case QEV_CLOSE_READ_HIGH:
				// error code: 1009
				qev_write(client, "\x88\x02\x03\xf1", 4);
				break;

			default:
				qev_write(client, "\x88\x00", 2);
				break;
		}
	} else {
		qev_write(client, HTTP_400, strlen(HTTP_400));
	}
}
