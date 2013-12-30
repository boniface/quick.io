/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

#define HANDSHAKE "/qio/ohai"

#define HEARTBEAT "\x00\x00\x00\x00\x00\x00\x00\x15""/qio/heartbeat:0=null"

static enum evs_status _heartbeat_cb(
	client_t *client G_GNUC_UNUSED,
	const evs_cb_t client_cb G_GNUC_UNUSED,
	gchar *json G_GNUC_UNUSED)
{
	return EVS_STATUS_OK;
}

enum protocol_handles protocol_raw_handles(
	struct client *client,
	void **data G_GNUC_UNUSED)
{
	gchar *str = client->qev_client.rbuff->str;

	if (g_strcmp0(str, HANDSHAKE) == 0) {
		return PROT_YES;
	}

	if (g_str_has_prefix(HANDSHAKE, str)) {
		return PROT_MAYBE;
	}

	return PROT_NO;
}

enum protocol_status protocol_raw_handshake(
	struct client *client,
	void *data G_GNUC_UNUSED)
{
	qev_write(client, HANDSHAKE, sizeof(HANDSHAKE) -1);
	g_string_truncate(client->qev_client.rbuff, 0);
	return PROT_OK;
}

enum protocol_status protocol_raw_route(struct client *client)
{
	guint64 len;
	enum protocol_status status;
	GString *rbuff = client->qev_client.rbuff;

	while (TRUE) {
		if (rbuff->len == 0) {
			return PROT_OK;
		}

		if (rbuff->len < sizeof(guint64)) {
			return PROT_AGAIN;
		}

		len = GUINT64_FROM_BE(*((guint64*)rbuff->str));
		if (rbuff->len < (len + sizeof(guint64))) {
			return PROT_AGAIN;
		}

		memmove(rbuff->str, rbuff->str + sizeof(guint64), len);

		status = protocol_raw_handle(client, len, len + sizeof(guint64));
		if (status != PROT_OK) {
			return status;
		}
	}
}

void protocol_raw_heartbeat(struct client *client, struct heartbeat *hb)
{
	protocol_raw_do_heartbeat(client, hb, HEARTBEAT, sizeof(HEARTBEAT) - 1);
}

GString* protocol_raw_frame(const gchar *data, const guint64 len)
{
	gchar size[sizeof(guint64)];
	GString *buff = qev_buffer_get();

	*((guint64*)size) = GUINT64_TO_BE(len);

	g_string_append_len(buff, size, sizeof(size));
	g_string_append_len(buff, data, len);

	return buff;
}

gboolean protocol_raw_check_handshake(struct client *client)
{
	gboolean good;
	GString *rbuff = client->qev_client.rbuff;

	good = g_strcmp0(rbuff->str, HANDSHAKE) == 0;
	g_string_truncate(rbuff, 0);

	return good;
}

void protocol_raw_do_heartbeat(
	struct client *client,
	struct heartbeat *hb,
	const gchar *heartbeat,
	const guint heartbeat_len)
{
	if (client->last_recv < hb->dead) {
		qev_close(client, RAW_HEARTATTACK);
		return;
	} else if (client->last_recv < hb->challenge) {
		evs_send_bruteforce(client, "/qio/heartbeat", NULL, NULL,
						_heartbeat_cb, NULL, NULL);
	} else if (client->last_send < hb->heartbeat) {
		qev_write(client, heartbeat, heartbeat_len);
	}

	client->last_send = qev_monotonic;
}

enum protocol_status protocol_raw_handle(
	struct client *client,
	const guint64 len,
	const guint64 frame_len)
{
	gchar *event_path;
	evs_cb_t client_cb;
	gchar *json;
	gchar *curr;
	gchar *end;
	GString *rbuff = client->qev_client.rbuff;
	gchar *str = rbuff->str;

	*(str + len) = '\0';

	curr = g_strstr_len(str, len, ":");
	if (curr == NULL) {
		goto error;
	}

	event_path = str;
	*curr = '\0';
	curr++;

	client_cb = g_ascii_strtoull(curr, &end, 10);
	if (*end != '=' || curr == end) {
		goto error;
	}

	json = end + 1;

	evs_route(client, event_path, client_cb, json);

	g_string_erase(rbuff, 0, frame_len);

	return PROT_OK;

error:
	qev_close(client, RAW_INVALID_EVENT_FORMAT);
	return PROT_FATAL;
}
