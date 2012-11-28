#include "qio.h"

status_t client_handshake(client_t *client) {
	status_t status = CLIENT_GOOD;
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
	if (!g_string_ends_with(buffer, HTTP_HEADER_TERMINATOR) && !g_string_ends_with(buffer, WEB_SOCKET_HEADER_TERMINATOR)) {
		return CLIENT_WAIT;
	}
	
	// http-parser requires a bit of setup to get going:
	// Headers go in a hash table for easy access
	// The path is just a string we hurl around
	// And we have to keep state while it's parsing up everything so we can populate everything
	
	gchar *headers_dup = g_strdup(buffer->str);
	GHashTable *headers = g_hash_table_new(g_str_hash, g_str_equal);
	gchar *path = NULL;
	
	int on_path(http_parser *parser, const char *at, gsize len) {
		path = headers_dup + ((gsize)at - (gsize)(buffer->str));
		*(path + len) = '\0';
		return 0;
	}
	
	// The previously-found key
	gchar *h_key;
	gsize h_len = 0;
	
	int on_key(http_parser *parser, const char *at, gsize len) {
		h_key = (gchar*)at;
		h_len = len;
		return 0;
	}
	
	int on_value(http_parser *parser, const char *at, gsize len) {
		if (h_len > 0) {
			gchar *header = headers_dup + ((gsize)h_key - (gsize)(buffer->str));
			gchar *value = headers_dup + ((gsize)at - (gsize)(buffer->str));
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
	
	if (http_parser_execute(&parser, &settings, buffer->str, buffer->len) == buffer->len) {
		// If the path is not /qio, it's not for us
		if (path == NULL || g_strstr_len(path, -1, QIO_PATH) == NULL) {
			status = CLIENT_FATAL;
		} else if (h_rfc6455_handles(path, headers)) {
			client->handler = h_rfc6455;
			status = h_rfc6455_handshake(client, headers);
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
	
	g_free(headers_dup);
	g_hash_table_unref(headers);
	
	// We read the header, no matter what was returned, it's our job to clear it
	g_string_truncate(client->message->socket_buffer, 0);
	
	return status;
}

status_t client_message(client_t* client) {
	status_t status;
	
	if (client->message->remaining_length) {
		switch (client->handler) {
			case h_rfc6455:
				status = h_rfc6455_continue(client);
				break;
			
			default:
			case h_none:
				// This should technically be impossible, nevertheless...
				CRITICAL("Client went into message processing without a handler");
				return CLIENT_FATAL;
		}
	} else {
		switch (client->handler) {
			case h_rfc6455:
				status = h_rfc6455_incoming(client);
				break;
			
			default:
			case h_none:
				CRITICAL("Client went into message processing without a handler");
				return CLIENT_FATAL;
		}
	}
	
	// If everything went well with the handler, process the message
	if (status == CLIENT_GOOD) {
		STATS_INC(messages_received);
		status = evs_server_handle(client);
	}
	
	return status;
}

status_t client_write(client_t *client, message_t *message) {
	// Extract the message from the client, if necessary
	if (message == NULL) {
		if (client->message == NULL) {
			return CLIENT_FATAL;
		}
		
		message = client->message;
	}
	
	// The actual frame to send to the client
	char *frame;
	
	// The amount of data to write to the socket
	gsize frame_len = 0;
	
	switch (client->handler) {
		case h_rfc6455:
			frame = h_rfc6455_prepare_frame_from_message(message, &frame_len);
			break;
		
		default:
			return CLIENT_FATAL;
	}
	
	if (frame == NULL) {
		return CLIENT_FATAL;
	}
	
	status_t status = client_write_frame(client, frame, frame_len);
	free(frame);
	return status;
}

status_t client_write_frame(client_t *client, char *frame, gsize frame_len) {
	STATS_INC(messages_sent);
	
	// Frames will ALWAYS be larger than 0, there MUST be a header
	// Since we're doing an equality check, casting to signed is all right
	if (frame_len > 0 && qev_write(client, frame, frame_len) == (gssize)frame_len) {
		return CLIENT_GOOD;
	} else {
		return CLIENT_FATAL;
	}
}

void client_ref(client_t *client) {
	g_atomic_pointer_add(&(client->ref_count), 1);
}

void client_unref(client_t *client) {
	if (g_atomic_pointer_add(&(client->ref_count), -1) == 1) {
		if (client->external_data != NULL) {
			g_hash_table_unref(client->external_data);
		}
		
		g_slice_free1(sizeof(*client), client);
	}
}

void client_set(client_t *client, const gchar *key, const gchar *value) {
	qev_client_lock(client);
	
	if (client->external_data == NULL) {
		client->external_data = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	}
	
	if (value == NULL) {
		g_hash_table_remove(client->external_data, key);
	} else {
		g_hash_table_insert(client->external_data, g_strdup(key), g_strdup(value));
	}
	
	qev_client_unlock(client);
}

gchar* client_get(client_t *client, const gchar *key) {
	qev_client_lock(client);
	
	gchar *value = NULL;
	if (client->external_data != NULL) {
		value = g_strdup(g_hash_table_lookup(client->external_data, key));
	}
	
	qev_client_unlock(client);
	
	return value;
}

gboolean client_has(client_t *client, const gchar *key) {
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