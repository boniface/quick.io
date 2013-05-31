#include "qio.h"

enum status client_handshake(struct client *client)
{
	static GPrivate priv_headers;

	enum status status = CLIENT_GOOD;
	GString *buffer = client->message->socket_buffer;

	// Now this is just absurd: flash sends a request for a policy file as:
	//    <policy-file-request/>
	// No headers, no newline, nothing.  Great way to talk to a server, flash.
	// We have to intercept the request here before we do any REAL checking.
	//
	// We use the \n\n check below to make sure that, if we didn't get a full XML
	// request, we're waiting for the finish.  No need to duplicate that logic here.
	if (h_flash_policy_handles(buffer->str)) {
		g_string_truncate(client->message->socket_buffer, 0);
		return h_flash_policy_handshake(client);
	}

	// If the client hasn't yet sent the terminating \n\n or \r\n\r\n, then just
	// don't parse anything, because it's not complete
	if (!g_string_ends_with(buffer, HTTP_HEADER_TERMINATOR) &&
		!g_string_ends_with(buffer, WEB_SOCKET_HEADER_TERMINATOR)) {

		return CLIENT_WAIT;
	}

	// http-parser requires a bit of setup to get going:
	// Headers go in a hash table for easy access
	// The path is just a string we hurl around
	// And we have to keep state while it's parsing up everything so we can
	// populate everything

	gchar *headers_str = buffer->str;
	gchar *path = NULL;

	// This is 5 times faster than creating a new header table
	// on each request
	GHashTable *headers = g_private_get(&priv_headers);
	if (G_UNLIKELY(headers == NULL)) {
		headers = g_hash_table_new(g_str_hash, g_str_equal);
		g_private_set(&priv_headers, headers);
	}

	int on_path(http_parser *parser, const gchar *at, gsize len) {
		path = buffer->str + ((gsize)at - (gsize)(buffer->str));
		*(path + len) = '\0';
		return 0;
	}

	// The previously-found key
	gchar *h_key;
	gsize h_len = 0;

	int on_key(http_parser *parser, const gchar *at, gsize len) {
		h_key = (gchar*)at;
		h_len = len;
		return 0;
	}

	int on_value(http_parser *parser, const gchar *at, gsize len) {
		if (h_len > 0) {
			gchar *header = headers_str + ((gsize)h_key - (gsize)(buffer->str));
			gchar *value = headers_str + ((gsize)at - (gsize)(buffer->str));
			*(header + h_len) = '\0';
			*(value + len) = '\0';

			g_hash_table_insert(headers, header, value);
		}

		h_len = 0;
		return 0;
	}

	// The callbacks used during parsing
	http_parser_settings settings;
	memset(&settings, 0, sizeof(settings));
	settings.on_url = on_path;
	settings.on_header_field = on_key;
	settings.on_header_value = on_value;

	http_parser parser;
	memset(&parser, 0, sizeof(parser));
	http_parser_init(&parser, HTTP_REQUEST);

	gsize len = http_parser_execute(&parser, &settings, buffer->str, buffer->len);
	if (len == buffer->len) {
		client->last_receive = qev_time;
		int flags = 0;

		// If the path is not /qio, it's not for us
		if (path == NULL || g_strstr_len(path, -1, QIO_PATH) == NULL) {
			status = CLIENT_FATAL;
		} else if ((flags = h_rfc6455_handles(path, headers)) != -1) {
			client->handler = h_rfc6455;
			status = h_rfc6455_handshake(client, flags, headers);
		} else {
			// No handler was found, and the headers were parsed OK, so
			// we just can't support this client
			status = CLIENT_FATAL;
		}
	} else {
		// If we can't parse the headers, then the client is being an idiot
		// So screw him, close the connection
		status = CLIENT_FATAL;
	}

	g_hash_table_remove_all(headers);

	// We read the header, no matter what was returned, it's our job to clear it
	g_string_truncate(client->message->socket_buffer, 0);

	return status;
}

enum status client_message(struct client *client)
{
	enum status status = CLIENT_ERROR;

	if (client->message->remaining_length) {
		switch (client->handler) {
			#define X(handler) \
				case handler: \
					status = handler## _continue(client); \
					break;

				HANDLERS
			#undef X

			case h_len:
			case h_none:
				CRITICAL("Client went into message processing without a handler");
				return CLIENT_FATAL;
		}
	} else {
		switch (client->handler) {
			#define X(handler) \
				case handler: \
					status = handler## _incoming(client); \
					break;

				HANDLERS
			#undef X

			case h_len:
			case h_none:
				CRITICAL("Client went into message processing without a handler");
				return CLIENT_FATAL;
		}
	}

	if (status == CLIENT_GOOD) {
		STATS_INC(client_message_received);
		client->last_receive = qev_time;
		status = evs_server_handle(client);
	}

	return status;
}

enum status client_write(struct client *client, struct message *message)
{
	if (message == NULL) {
		if (client->message == NULL) {
			return CLIENT_FATAL;
		}

		message = client->message;
	}

	if (message->buffer->len == 0) {
		return CLIENT_GOOD;
	}

	// The actual frame to send to the client
	gchar *frame = NULL;

	// The amount of data to write to the socket
	gsize frame_len = 0;

	switch (client->handler) {
		#define X(handler) \
			case handler: \
				frame = handler## _prepare_frame_from_message(message, &frame_len); \
				break;

			HANDLERS
		#undef X

		case h_len:
		case h_none:
			return CLIENT_FATAL;
	}

	if (frame == NULL) {
		return CLIENT_FATAL;
	}

	STATS_INC(client_message_sent);
	enum status status = client_write_frame(client, frame, frame_len);

	g_free(frame);
	return status;
}

void client_write_close(struct client *client)
{
	gchar *frame = NULL;
	gsize frame_len = 0;

	switch (client->handler) {
		#define X(handler) \
			case handler: \
				frame = handler## _close_frame(&frame_len); \
				break;

			HANDLERS
		#undef X

		case h_len:
		case h_none:
			return;
	}

	client_write_frame(client, frame, frame_len);
}

enum status client_write_frame(struct client *client, gchar *frame, gsize frame_len)
{
	// Frames MUST ALWAYS be larger than 0, there MUST be a header
	// Since we're doing an equality check, casting to signed is all right
	if (frame_len > 0 && qev_write(client, frame, frame_len) == (gssize)frame_len) {
		client->last_send = qev_time;
		return CLIENT_GOOD;
	} else {
		STATS_INC(client_failed_writes);
		return CLIENT_FATAL;
	}
}

void client_ref(struct client *client)
{
	g_atomic_int_inc(&client->ref_count);
}

void client_unref(struct client *client)
{
	if (g_atomic_int_dec_and_test(&client->ref_count)) {
		// Allow the close handler to access client data before freeing
		GHashTable *data = client->external_data;

		conns_client_free(client);

		if (data != NULL) {
			g_hash_table_unref(data);
		}
	}
}

const gchar* client_set(struct client *client, const gchar *key, const gchar *value)
{
	qev_client_lock(client);

	if (client->external_data == NULL) {
		client->external_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	}

	gchar *ret = NULL;
	if (value == NULL) {
		g_hash_table_remove(client->external_data, key);
	} else {
		ret = g_strdup(value);
		g_hash_table_insert(client->external_data, g_strdup(key), ret);
	}

	qev_client_unlock(client);

	return ret;
}

const gchar* client_get(struct client *client, const gchar *key)
{
	qev_client_lock(client);

	gchar *value = NULL;
	if (client->external_data != NULL) {
		value = g_strdup(g_hash_table_lookup(client->external_data, key));
	}

	qev_client_unlock(client);

	return value;
}

gboolean client_has(struct client *client, const gchar *key)
{
	qev_client_lock(client);

	gboolean has = FALSE;
	if (client->external_data != NULL) {
		has = g_hash_table_contains(client->external_data, key);
	}

	qev_client_unlock(client);

	return has;
}

#ifdef TESTING
#include "../test/test_client.c"
#endif
