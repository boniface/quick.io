/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

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

static qev_stats_counter_t *_stat_handshakes_qio;
static qev_stats_counter_t *_stat_handshakes_qio_invalid;
static qev_stats_timer_t *_stat_route_time;

static enum protocol_status _decode(
	struct client *client,
	const guint64 offset,
	guint64 *len_,
	guint64 *frame_len)
{
	/*
	 * See http://tools.ietf.org/html/rfc6455#section-5.2 for more on what's
	 * going on here
	 */

	guint64 i;
	guint64 len;
	guint64 max;
	gchar mask[4];
	union {
		gchar c[sizeof(__uint128_t)];
		__uint128_t i;
	} mask128;
	GString *rbuff = client->qev_client.rbuff;
	gchar *str = rbuff->str + offset;
	gchar *msg = rbuff->str + offset;
	guint64 rbuff_len = rbuff->len - offset;
	guint16 header_len = 0;

	if ((str[0] & OPCODE) == OPCODE_CLOSE) {
		qev_close(client, QEV_CLOSE_HUP);
		return PROT_FATAL;
	}

	if (rbuff_len < 6) {
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
		if (rbuff_len < header_len) {
			return PROT_AGAIN;
		}

		len = GUINT16_FROM_BE(*((guint16*)(str + 2)));
		memcpy(mask, str + 4, 4);
	} else {
		header_len = 14;
		if (rbuff_len < header_len) {
			return PROT_AGAIN;
		}

		len = GUINT64_FROM_BE(*((guint64*)(str + 2)));
		memcpy(mask, str + 10, 4);
	}

	if (rbuff_len < (header_len + len)) {
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

void protocol_rfc6455_init()
{
	_stat_handshakes_qio = qev_stats_counter("protocol.rfc6455",
								"handshakes.qio", TRUE);
	_stat_handshakes_qio_invalid = qev_stats_counter("protocol.rfc6455",
								"handshakes.qio_invalid", TRUE);
	_stat_route_time = qev_stats_timer("protocol.rfc6455", "route");
}

enum protocol_handles protocol_rfc6455_handles(struct client *client G_GNUC_UNUSED)
{
	return PROT_NO;
}

enum protocol_status protocol_rfc6455_handshake(struct client *client)
{
	guint64 len;
	guint64 frame_len;
	gboolean good;
	enum protocol_status status;

	status = _decode(client, 0, &len, &frame_len);
	if (status == PROT_OK) {
		good = protocol_raw_check_handshake(client);
		if (good) {
			qev_stats_counter_inc(_stat_handshakes_qio);
			qev_write(client, QIO_HANDSHAKE, sizeof(QIO_HANDSHAKE) - 1);
			status = PROT_OK;
		} else {
			qev_stats_counter_inc(_stat_handshakes_qio_invalid);
			qev_close(client, QIO_CLOSE_INVALID_HANDSHAKE);
			status = PROT_FATAL;
		}
	}

	return status;
}

enum protocol_status protocol_rfc6455_route(struct client *client, gsize *used)
{
	guint64 len;
	guint64 frame_len;
	enum protocol_status status;
	GString *rbuff = client->qev_client.rbuff;

	qev_stats_time(_stat_route_time, {
		status = _decode(client, *used, &len, &frame_len);
		if (status == PROT_OK) {
			gchar *start = rbuff->str + *used;
			start[len] = '\0';
			status = protocol_raw_handle(client, start);
			*used += frame_len;
		}
	});

	return status;
}

void protocol_rfc6455_heartbeat(
	struct client *client,
	const struct protocol_heartbeat *hb)
{
	protocol_raw_do_heartbeat(client, hb,
					HEARTBEAT, sizeof(HEARTBEAT) - 1);
}

struct protocol_frames protocol_rfc6455_frame(
	const gchar *ev_path,
	const gchar *ev_extra,
	const evs_cb_t server_cb,
	const gchar *json)
{
	GString *def = qev_buffer_get();
	GString *raw = protocol_raw_format(ev_path, ev_extra, server_cb, json);
	const gsize len = raw->len;

	g_string_append_c(def, '\x81');

	if (len <= PAYLOAD_SHORT) {
		g_string_append_c(def, (guint8)len);
	} else if (len <= 0xffff) {
		guint16 belen = GUINT16_TO_BE(len);
		g_string_append_c(def, (gchar)PAYLOAD_MEDIUM);
		qev_buffer_append_len(def, (gchar*)&belen, sizeof(belen));
	} else {
		guint64 belen = GUINT64_TO_BE(len);
		g_string_append_c(def, (gchar)PAYLOAD_LONG);
		qev_buffer_append_len(def, (gchar*)&belen, sizeof(belen));
	}

	qev_buffer_append_len(def, raw->str, raw->len);

	qev_buffer_put(raw);

	return (struct protocol_frames){
		.def = def,
		.raw = NULL,
	};
}

void protocol_rfc6455_close(struct client *client, guint reason)
{
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
}
