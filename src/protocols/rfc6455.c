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
#define VERSION_KEY "Sec-WebSocket-Version"

/**
 * The header key that points to the challenge key
 */
#define CHALLENGE_KEY "Sec-WebSocket-Key"

/**
 * The header key that points to what subprotocol is being used
 */
#define PROTOCOL_KEY "Sec-WebSocket-Protocol"

/**
 * Completely disables caching for HTTP requests
 */
#define HTTP_NOCACHE \
	"Cache-Control: no-cache, no-store, must-revalidate\r\n" \
	"Pragma: no-cache\r\n" \
	"Expires: 0\r\n\r\n"

#define HTTP_400 \
	"HTTP/1.1 400 Bad Request\r\n" \
	"Connection: close\r\n" \
	HTTP_NOCACHE

#define HTTP_101 \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Protocol: quickio\r\n" \
	"Sec-WebSocket-Accept: %s\r\n" \
	HTTP_NOCACHE

#define HTTP_HEADER_TERMINATOR "\n\n"
#define WEB_SOCKET_HEADER_TERMINATOR "\r\n\r\n"

#define QIO_HANDSHAKE "\x81\x09/qio/ohai"

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

#define HEARTBEAT "\x81""\x15""/qio/heartbeat:0=null"

/**
 * Indicates that a client was speaking HTTP but was not intended for us or had
 * some other error (eg. missing headers) and should be terminated.
 */
#define HTTP_UNSUPPORTED 0x4000

/**
 * Indicates that a client has finished the HTTP part of the handshake
 * but still needs to complete the QIO handshake
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

static gint _parser_on_key(http_parser *parser, const gchar *at, gsize len)
{
	struct _parser_data *pdata = parser->data;

	pdata->key = (gchar*)at;
	pdata->key_len = len;

	return 0;
}

static gint _parser_on_value(http_parser *parser, const gchar *at, gsize len)
{
	gchar *header;
	gchar *value;
	struct _parser_data *pdata = parser->data;

	if (pdata->key_len > 0) {
		header = pdata->key;
		*(header + pdata->key_len) = '\0';

		value = ((gchar*)at);
		*(value + len) = '\0';

		g_hash_table_insert(pdata->headers, header, value);
	}

	pdata->key_len = 0;

	return 0;
}

static GHashTable* _parse_headers(GString *rbuff)
{
	static __thread GHashTable *headers;

	gsize len;
	http_parser parser;
	http_parser_settings settings;
	struct _parser_data pdata;

	if (G_UNLIKELY(headers == NULL)) {
		headers = g_hash_table_new(g_str_hash, g_str_equal);
		qev_cleanup_thlocal(headers, (qev_free_fn)g_hash_table_unref);
	}

	g_hash_table_remove_all(headers);

	memset(&pdata, 0, sizeof(pdata));
	memset(&settings, 0, sizeof(settings));

	settings.on_header_field = _parser_on_key;
	settings.on_header_value = _parser_on_value;

	pdata.buffer = rbuff;
	pdata.headers = headers;

	http_parser_init(&parser, HTTP_REQUEST);
	parser.data = &pdata;
	len = http_parser_execute(&parser, &settings, rbuff->str, rbuff->len);

	if (len != rbuff->len) {
		return NULL;
	}

	if (!parser.upgrade) {
		return NULL;
	}

	return headers;
}

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
	gchar mask[4];

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

	if (rbuff->len < len) {
		return PROT_AGAIN;
	}

	*len_ = len;
	*frame_len = len + header_len;
	str += header_len;

	for (i = 0; i < len; i++) {
		// @todo __builtin_prefetch, will that help at all?
		msg[i] = str[i] ^ mask[i & 3];
	}

	*(msg + len) = '\0';

	if (!g_utf8_validate(msg, len, NULL)) {
		qev_close(client, RFC6455_NOT_UTF8);
		return PROT_FATAL;
	}

	return PROT_OK;
}

static void _handshake_http(
	struct client *client,
	GHashTable *headers)
{
	gsize size;
	gsize b64len;
	gint state = 0;
	gint save = 0;
	GChecksum *sha1 = g_checksum_new(G_CHECKSUM_SHA1);
	GString *sha = qev_buffer_get();
	GString *b64 = qev_buffer_get();
	const gchar *key = g_hash_table_lookup(headers, CHALLENGE_KEY);

	g_string_append(sha, key);
	g_string_append(sha, HASH_KEY);

	size = sha->len;
	g_checksum_update(sha1, (guchar*)sha->str, sha->len);
	g_checksum_get_digest(sha1, (guchar*)sha->str, &size);
	g_string_set_size(sha, size);

	/*
	 * This line brought to your courtesy of "wut". Wut, for when you know
	 * you have to do something but just take their word for it.
	 * See: https://developer.gnome.org/glib/2.37/glib-Base64-Encoding.html#g-base64-encode-step
	 */
	g_string_set_size(b64, (size / 3 + 1) * 4 + 1);

	b64len = g_base64_encode_step((guchar*)sha->str, size, FALSE, b64->str,
									&state, &save);
	b64len += g_base64_encode_close(FALSE, b64->str + b64len, &state, &save);
	g_string_set_size(b64, b64len);

	qev_writef(client, HTTP_101, b64->str);

	g_checksum_free(sha1);
	qev_buffer_put(sha);
	qev_buffer_put(b64);

	g_string_truncate(client->qev_client.rbuff, 0);
}

static enum protocol_status _handshake_qio(struct client *client)
{
	enum protocol_status status;
	gboolean good;
	guint64 len;
	guint64 frame_len;

	status = _decode(client, &len, &frame_len);
	if (status != PROT_OK) {
		return status;
	}

	good = protocol_raw_check_handshake(client);
	if (good) {
		qev_write(client, QIO_HANDSHAKE, sizeof(QIO_HANDSHAKE) - 1);
		status = PROT_OK;
	} else {
		qev_close(client, QIO_CLOSE_INVALID_HANDSHAKE);
		status = PROT_FATAL;
	}

	return status;
}

enum protocol_handles protocol_rfc6455_handles(
	struct client *client,
	void **data)
{
	/**
	 * This is a thread-local value that is only valid for this leg
	 * of the handshake
	 */
	GHashTable *headers;
	GString *rbuff = client->qev_client.rbuff;

	if (!g_str_has_suffix(rbuff->str, WEB_SOCKET_HEADER_TERMINATOR) &&
		!g_str_has_suffix(rbuff->str, HTTP_HEADER_TERMINATOR)) {
		return PROT_MAYBE;
	}

	if (!g_str_has_prefix(rbuff->str, "GET")) {
		return PROT_NO;
	}

	/*
	 * Once we start messing with the headers, we're claiming the client, so
	 * PROT_YES is our only return value after this point.
	 */
	headers = _parse_headers(rbuff);

	if (headers == NULL ||
		g_strcmp0(g_hash_table_lookup(headers, PROTOCOL_KEY), "quickio") != 0 ||
		!g_hash_table_contains(headers, CHALLENGE_KEY) ||
		g_strcmp0(g_hash_table_lookup(headers, VERSION_KEY), "13") != 0) {

		client->protocol_flags |= HTTP_UNSUPPORTED;
	}

	*data = headers;

	return PROT_YES;
}

enum protocol_status protocol_rfc6455_handshake(
	struct client *client,
	void *data)
{
	/*
	 * This flag is set in handles(): must wait for the protocol
	 * to be set before closing so that an error message can
	 * be sent back to the client.
	 */
	if (client->protocol_flags & HTTP_UNSUPPORTED) {
		qev_close(client, QIO_CLOSE_NOT_SUPPORTED);
		return PROT_FATAL;
	}

	if (!(client->protocol_flags & HTTP_HANDSHAKED)) {
		GHashTable *headers = data;
		_handshake_http(client, headers);

		client->protocol_flags |= HTTP_HANDSHAKED;

		return PROT_AGAIN;
	} else {
		return _handshake_qio(client);
	}
}

enum protocol_status protocol_rfc6455_route(struct client *client)
{
	guint64 len;
	guint64 frame_len;
	enum protocol_status status = _decode(client, &len, &frame_len);

	if (status == PROT_OK) {
		status = protocol_raw_handle(client, len, frame_len);
	}

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
		g_string_append_len(f, (gchar*)&belen, sizeof(belen));
	} else {
		guint64 belen = GUINT64_TO_BE(len);
		g_string_append_c(f, (gchar)PAYLOAD_LONG);
		g_string_append_len(f, (gchar*)&belen, sizeof(belen));
	}

	g_string_append_len(f, e->str, e->len);

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
				// error code: 1008
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
		qev_write(client, HTTP_400, sizeof(HTTP_400) - 1);
	}
}
