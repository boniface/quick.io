/**
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include "quickio.h"

#define HANDSHAKE "/qio/ohai"

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
	gboolean good = protocol_raw_check_handshake(client);
	enum protocol_status status;

	if (good) {
		qev_write(client, HANDSHAKE, sizeof(HANDSHAKE) -1);
		status = PROT_OK;
	} else {
		status = PROT_FATAL;
	}

	return status;
}

enum protocol_status protocol_raw_route(struct client *client)
{
	guint64 len;
	GString *rbuff = client->qev_client.rbuff;

	if (rbuff->len < sizeof(guint64)) {
		return PROT_AGAIN;
	}

	len = GUINT64_FROM_BE(*((guint64*)rbuff->str));
	if (rbuff->len < len) {
		return PROT_AGAIN;
	}

	g_string_erase(rbuff, 0, sizeof(guint64));

	return protocol_raw_handle(client, len, len);
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

	g_string_erase(client->qev_client.rbuff, 0, frame_len);

	return PROT_OK;

error:
	qev_close(client, RAW_INVALID_EVENT_FORMAT);
	return PROT_FATAL;
}