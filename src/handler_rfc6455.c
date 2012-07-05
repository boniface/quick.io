#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "commands.h"
#include "debug.h"
#include "handler_rfc6455.h"
#include "handler_rfc6455_const.h"
#include "socket.h"

/**
 * The corresponding RFC: http://tools.ietf.org/html/rfc6455
 */

/**
 * Read masked text from the socket buffer.
 */
static status_t _rfc6455_read(client_t *client, int header_len) {
	message_t *message = client->message;
	char *buff = message->socket_buffer->str;
	
	// Advance the buffer past the header, we can't read it
	buff += header_len;
	
	// It's possible we have more on the socket buffer than this message, so only
	// read the message.  It's also possible the buffer doesn't contain the whole
	// message yet, so only go for the smallest of what we can use.
	gsize remaining = MIN(message->socket_buffer->len - header_len, message->remaining_length);
	
	// Transform the 32bit int to a char array for simpler use
	char *mask = (char*)(&message->mask);
	
	// For quicker access
	GString *ob = message->buffer;
	
	// The mask location depends on how much we have already read
	gsize mask_loc = ob->len;
	
	// Run through the remainder of the string and throw it to the buffer
	for (gsize i = 0; i < remaining; i++) {
		g_string_append_c(ob, (char)(*(buff + i) ^ mask[mask_loc++ % 4]));
	}
	
	// Update the current command's mask and length for any future reads
	client->message->remaining_length -= remaining;
	
	// Remove what we just processed from the socket buffer
	g_string_erase(message->socket_buffer, 0, header_len + remaining);
	
	// Are there any bytes that haven't been read from the socket that we need from this
	// message?  If len - available != 0, then the entire message isn't yet in our buffer.
	if (client->message->remaining_length != 0) {
		return CLIENT_WAIT;
	}
	
	return CLIENT_GOOD;
}

/**
 * Start reading from a masked buffer, and get all the info
 * needed so that _rfc6455_read() can run.
 */
static status_t _rfc6455_start(client_t *client) {
	GString *sb = client->message->socket_buffer;
	char *buff = sb->str;
	
	// If we haven't even finished reading the header from the socket
	if (sb->len < HEADER_LEN) {
		return CLIENT_WAIT;
	}
	
	// The first 7 bits of the second byte tell us the length
	// You can't make this stuff up :(
	int len = *(buff + 1) & SECOND_BYTE;
	
	// The 32bit mask data: position dependent based on the payload len
	char *mask;
	
	// The length of the header for this message
	short header_len;
	
	if (len <= PAYLOAD_SHORT) {
		// Length is good, now all we need is the mask (right after the headers)
		mask = buff + HEADER_LEN;
		
		// Skip the headers and the mask
		header_len = HEADER_LEN + MASK_LEN;
	} else if (len == PAYLOAD_LONG) {
		// We haven't finished reading the extended header from the socket
		if (sb->len < EXTENDED_HEADER_LEN) {
			return CLIENT_WAIT;
		}
		
		// The third and fourth bytes contain the length
		len += (guint16)(*(buff + HEADER_LEN));
		
		// The mask starts after the header and extended length
		mask = buff + EXTENDED_HEADER_LEN;
		
		// Skip the headers, the extended length, and the mask
		header_len = EXTENDED_HEADER_LEN + MASK_LEN;
	} else {
		len = MAX_MESSAGE_LENGTH + 1;
	}
	
	// Advance the buffer to the beginning of the message
	buff += header_len;
	
	// Don't accept the incoming data if it's too long
	if (len > MAX_MESSAGE_LENGTH) {
		return CLIENT_MESSAGE_TOO_LONG;
	}
	
	// Pass on the length that is left to read to the reader
	client->message->remaining_length = len;
	
	// Pass on the mask as an int to make it easier for memory management
	client->message->mask = (*((guint32*)mask));
	
	return _rfc6455_read(client, header_len);
}

gboolean rfc6455_handles(SoupMessageHeaders *req_headers) {
	const char *id = soup_message_headers_get_one(req_headers, VERSION_KEY);
	return id != NULL && strncmp(id, "13", 2) == 0;
}

gboolean rfc6455_handshake(client_t *client, SoupMessageHeaders *req_headers) {
	const char *key = soup_message_headers_get_one(req_headers, CHALLENGE_KEY);
	
	if (key == NULL) {
		return FALSE;
	}
	
	//build up the concated key, ready for hashing for the return header
	guchar out[strlen(key) + HASH_KEY_LEN];
	strcpy((char*)out, key);
	strcat((char*)out, HASH_KEY);
	
	gsize size = strlen((char*)out);
	GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA1);
	
	// Do some hard-core hashing, then free it all
	g_checksum_update(sum, out, size);
	g_checksum_get_digest(sum, out, &size);
	gchar *b64 = g_base64_encode(out, size);
	g_checksum_free(sum);
	
	size = HEADERS_LEN + strlen(b64);
	char headers[size];
	
	snprintf(headers, size, HEADERS, b64);
	g_free(b64);
	
	if (client_write_frame(client, headers, strlen(headers)) != CLIENT_GOOD) {
		return FALSE;
	}
	
	return TRUE;
}

char* rfc6455_prepare_frame(opcode_t type, gchar *payload, int payload_len, int *frame_len) {
	char *frame;
	
	// If 125 chars or less, then only use 7 bits to represent
	// payload length
	//
	// If greater than 125, then the first 7 bits should be 126,
	// and the following two bytes will be interpreted as a
	// 16-bit unsigned int
	if (payload_len <= PAYLOAD_SHORT) {
		// Since this is a small message, we only need 2 extra bytes
		// to represent the data
		*frame_len = (payload_len + 2);
		
		frame = malloc(*frame_len * sizeof(*frame));
		
		// If can't malloc...that's bad.
		if (frame == NULL) {
			*frame_len = -1;
			return NULL;
		}
		
		// The first frame of text is ALWAYS the same
		*frame = FIRST_BYTE;
		
		// The opcode that we should sent back to the client
		switch (type) {
			case op_pong:
				*frame |= OP_PONG;
				break;
			
			default:
			case op_text:
				*frame |= OP_TEXT;
				break;
		}
		
		// The second frame: the first bit is always 0 (data going
		// to the client is never masked), and since the length is
		// less than 125, that bit will ALWAYS be set to 0, as it should
		*(frame + 1) = payload_len;
		
		memcpy((frame + 2), payload, payload_len);
	} else {
		ERROR("Msg len > 125, not implemented");
		*frame_len = -1;
		return NULL;
	}
	
	return frame;
}

char* rfc6455_prepare_frame_from_message(message_t *message, int *frame_len) {
	return rfc6455_prepare_frame(
		message->type,
		message->buffer->str,
		message->buffer->len,
		frame_len
	);
}

status_t rfc6455_continue(client_t *client) {
	// There are no headers when continuing
	status_t status = _rfc6455_read(client, 0);
	
	if (status != CLIENT_GOOD) {
		return status;
	}
	
	// If the message is done, and it's a ping, then we just need to write
	// directly back to the client what was recieved
	if (client->message->type == op_pong) {
		return CLIENT_WRITE;
	}
	
	return status;
}

status_t rfc6455_incoming(client_t *client) {
	char *buff = client->message->socket_buffer->str;
	
	// If data came from the client unmasked, then that's wrong. Abort.
	// There MUST always be at least the first byte, so we don't need
	// any continuation logic here
	if ((*(buff + 1) & MASK_BIT) == 0) {
		return CLIENT_NEED_MASK;
	}
	
	// Remove everything but the OPCODE from the byte
	char opcode = *buff & OPCODE;
	
	if (opcode == OP_TEXT) {
		client->message->type = op_text;
		return _rfc6455_start(client);
	} else if (opcode == OP_CONTINUATION) {
		return _rfc6455_start(client);
	} else if (opcode == OP_PING) {
		client->message->type = op_pong;
		
		// If the message wasn't correctly processed, then return that
		status_t status;
		if ((status =_rfc6455_start(client)) != CLIENT_GOOD) {
			return status;
		}
		
		// If we got a good status, then we need to respond to the ping
		return CLIENT_WRITE;
	}
	
	// If the client wasn't handled above, that was bad, we don't support it
	return CLIENT_UNSUPPORTED_OPCODE;
}