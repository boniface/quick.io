#include "qio.h"

// From: http://tools.ietf.org/html/rfc6455#section-1.3
#define HASH_KEY "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define HASH_KEY_LEN sizeof(HASH_KEY) - 1 //-1 to kill the null terminator

// Headers used in rfc6455
#define VERSION_KEY "Sec-WebSocket-Version"
#define CHALLENGE_KEY "Sec-WebSocket-Key"

// Response headers
#define HEADERS "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: %s\r\n\r\n"
#define HEADERS_LEN sizeof(HEADERS)

// Opcodes that can be used
#define OPCODE 0x0F
#define OP_CONTINUATION 0x00
#define OP_TEXT 0x01
#define OP_BINARY 0x02
#define OP_CLOSE 0x08
#define OP_PING 0x09
#define OP_PONG 0x0A

// The location of the mask bit in the second byte
#define MASK_BIT 0b10000000

// First bit = this is the final frame
#define FIRST_BYTE 0b10000000

// Ignore the first bit of the second byte, it's the masking bit
#define SECOND_BYTE 0b01111111

// The value indicating we're sending a 16-bit unsigned int as the length
#define SECOND_BYTE_16BIT_LEN 126

// The different payload lengths that change the header
#define PAYLOAD_LEN_SHORT 125
#define PAYLOAD_LEN_LONG 126
#define PAYLOAD_LEN_DEADLY 127

#define PAYLOAD_SHORT 125
#define PAYLOAD_LONG 0xFFFF

// Mask is 32 bits
#define MASK_LEN (32 / 8)

// Header is 16 bits
#define HEADER_LEN (16 / 8)

// Extended-length header contains an extra 16 bits for payload len
#define EXTENDED_HEADER_LEN (HEADER_LEN + (16 / 8))

// The frame sent once a client closes
#define CLOSE_FRAME ("\x88" "\x00")

/**
 * Read masked text from the socket buffer.
 */
static status_t _h_rfc6455_read(client_t *client, int header_len) {
	message_t *message = client->message;
	char *sbuff = message->socket_buffer->str;

	// Advance the buffer past the header, we can't read it
	sbuff += header_len;

	// It's possible we have more on the socket buffer than this message, so only
	// read the message.  It's also possible the buffer doesn't contain the whole
	// message yet, so only go for the smallest of what we can use.
	gsize remaining = MIN(message->socket_buffer->len - header_len, message->remaining_length);

	// Transform the 32bit int to a char array for simpler use (read: offsets)
	char *mask = (char*)(&message->mask);

	// The mask location depends on how much we have already read
	gsize position = message->buffer->len;

	// g_string_set_size() will add the NULL byte, so no +1 here
	gsize required_len = message->buffer->len + remaining;

	// Access the string directly, it's a bit faster
	// But make sure its internal length is set correctly and that there's enough space
	g_string_set_size(message->buffer, required_len);

	gchar *ob = message->buffer->str;
	for (gsize i = 0; i < remaining; i++, position++) {
		ob[position] = (char)(*(sbuff + i) ^ mask[position & 3]);
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
 * needed so that _h_rfc6455_read() can run.
 */
static status_t _h_rfc6455_start(client_t *client) {
	GString *sb = client->message->socket_buffer;
	char *buff = sb->str;

	// The first 7 bits of the second byte tell us the length
	// You can't make this stuff up :(
	guint16 len = *(buff + 1) & SECOND_BYTE;

	// The 32bit mask data: position dependent based on the payload len
	char *mask = NULL;

	// The length of the header for this message
	guint16 header_len = 0;

	if (len <= PAYLOAD_LEN_SHORT) {
		// Length is good, now all we need is the mask (right after the headers)
		mask = buff + HEADER_LEN;

		// Skip the headers and the mask
		header_len = HEADER_LEN + MASK_LEN;
	} else if (len == PAYLOAD_LEN_LONG) {
		// We haven't finished reading the extended header from the socket
		if (sb->len < EXTENDED_HEADER_LEN) {
			return CLIENT_WAIT;
		}

		// The third and fourth bytes contain the length
		len = GUINT16_FROM_BE(*((guint16*)(buff + HEADER_LEN)));

		// The mask starts after the header and extended length
		mask = buff + EXTENDED_HEADER_LEN;

		// Skip the headers, the extended length, and the mask
		header_len = EXTENDED_HEADER_LEN + MASK_LEN;
	} else {
		len = option_max_message_size() + 1;
	}

	// Advance the buffer to the beginning of the message
	buff += header_len;

	// Don't accept the incoming data if it's too long
	if (len > option_max_message_size()) {
		return CLIENT_FATAL;
	}

	// Wait on more information if we didn't completely read the header
	if (sb->len < header_len) {
		return CLIENT_WAIT;
	}

	// Pass on the length that is left to read to the reader
	client->message->remaining_length = len;

	// Pass on the mask as an int to make it easier for memory management
	client->message->mask = *((guint32*)mask);

	return _h_rfc6455_read(client, header_len);
}

gboolean h_rfc6455_handles(gchar *path, GHashTable *headers) {
	const char *id = g_hash_table_lookup(headers, VERSION_KEY);
	return id != NULL && strncmp(id, "13", 2) == 0;
}

status_t h_rfc6455_handshake(client_t *client, GHashTable *headers) {
	const char *key = g_hash_table_lookup(headers, CHALLENGE_KEY);

	if (key == NULL) {
		return CLIENT_FATAL;
	}

	// Build up the concated key, ready for hashing for the return header
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

	g_string_printf(client->message->buffer, HEADERS, b64);
	g_free(b64);

	return CLIENT_WRITE;
}

char* h_rfc6455_prepare_frame(opcode_t type, gboolean masked, gchar *payload, guint64 payload_len, gsize *frame_len) {
	char *frame;

	// If masked, then start the header off with room from the mask
	guint8 header_size = masked ? MASK_LEN : 0;

	// If 125 chars or less, then only use 7 bits to represent
	// payload length
	//
	// If greater than 125, then the first 7 bits should be 126,
	// and the following two bytes will be interpreted as a
	// 16-bit unsigned int
	if (payload_len <= PAYLOAD_SHORT) {
		// Since this is a small message, we only need 2 extra bytes
		// to represent the data
		header_size += HEADER_LEN;
		*frame_len = payload_len + header_size;

		frame = g_malloc0(*frame_len * sizeof(*frame));

		// The second frame: the first bit is always 0 (data going
		// to the client is never masked), and since the length is
		// less than 125, that bit will ALWAYS be set to 0, as it should
		*(frame + 1) = payload_len;
	} else if (payload_len <= PAYLOAD_LONG) {
		// We need an extra 2 bytes, for 4 bytes in total,
		// to provide the header for this message
		header_size += EXTENDED_HEADER_LEN;
		*frame_len = payload_len + header_size;

		frame = g_malloc0(*frame_len * sizeof(*frame));

		// The second frame: the first bit is always 0 (data going
		// to the client is never masked), and to indicate that we
		// are sending a 16-bit payload, we need to set this to 126,
		// which will also set the masking bit to 0
		*(frame + 1) = SECOND_BYTE_16BIT_LEN;
		*((guint16*)(frame + 2)) = GUINT16_TO_BE(payload_len);
	} else {
		CRITICAL("Msg len > 65535, not implemented");
		return NULL;
	}

	// The first frame of text is ALWAYS the same
	*frame = FIRST_BYTE;

	// The opcode that should be sent back to the client
	switch (type) {
		case op_pong:
			*frame |= OP_PONG;
			break;

		default:
		case op_text:
			*frame |= OP_TEXT;
			break;
	}

	if (masked) {
		// The length was set above, so this is safe
		*(frame + 1) |= MASK_BIT;

		// The mask can just be a random 32bit int, then we trick it into some bytes
		gint32 randmask = g_random_int();
		char *mask = (char*)&randmask;

		// The mask comes directly after the header
		memcpy((frame + 2), mask, MASK_LEN);

		// The start of the encoded bytes
		char *start = frame + header_size;

		for (guint64 i = 0; i < payload_len; i++) {
			*(start + i) = *(payload + i) ^ *(mask + (i % MASK_LEN));
		}
	} else {
		memcpy((frame + header_size), payload, payload_len);
	}

	return frame;
}

char* h_rfc6455_prepare_frame_from_message(message_t *message, gsize *frame_len) {
	return h_rfc6455_prepare_frame(
		message->type,
		FALSE,
		message->buffer->str,
		message->buffer->len,
		frame_len
	);
}

status_t h_rfc6455_continue(client_t *client) {
	// There are no headers when continuing
	status_t status = _h_rfc6455_read(client, 0);

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

status_t h_rfc6455_incoming(client_t *client) {
	// If we haven't even finished reading the header from the socket
	if (client->message->socket_buffer->len < HEADER_LEN) {
		return CLIENT_WAIT;
	}

	char *buff = client->message->socket_buffer->str;

	// If data came from the client unmasked, then that's wrong. Abort.
	// There MUST always be at least the first byte, so we don't need
	// any continuation logic here
	//
	// See: http://tools.ietf.org/html/rfc6455#section-5.1
	if ((*(buff + 1) & MASK_BIT) == 0) {
		return CLIENT_FATAL;
	}

	// Remove everything but the OPCODE from the byte
	char opcode = *buff & OPCODE;

	if (opcode == OP_TEXT) {
		client->message->type = op_text;
		return _h_rfc6455_start(client);
	} else if (opcode == OP_CONTINUATION) {
		// Continuation frames are too complicated to implement at the moment,
		// and I don't see myself needing them, so I'm just not going to implement
		// them.
		//
		// From: http://tools.ietf.org/html/rfc6455#section-5.4
		// Example: EXAMPLE: For a text message sent as three fragments, the first
		// fragment would have an opcode of 0x1 and a FIN bit clear, the
		// second fragment would have an opcode of 0x0 and a FIN bit clear,
		// and the third fragment would have an opcode of 0x0 and a FIN bit
		// that is set.
		//
		// This is way too complicated.  Seriously, why?
		return CLIENT_FATAL;
		// return _h_rfc6455_start(client);
	} else if (opcode == OP_CLOSE) {
		return CLIENT_FATAL;
	} else if (opcode == OP_PING) {
		client->message->type = op_pong;

		// If the message wasn't correctly processed, then return that
		status_t status;
		if ((status =_h_rfc6455_start(client)) != CLIENT_GOOD) {
			return status;
		}

		// If we got a good status, then we need to respond to the ping
		return CLIENT_WRITE;
	}

	// If the client wasn't handled above, that was bad, we don't support it
	return CLIENT_FATAL;
}

char* h_rfc6455_close_frame(gsize *frame_len) {
	*frame_len = sizeof(CLOSE_FRAME)-1;
	return (char*)CLOSE_FRAME;
}

#ifdef TESTING
#include "../test/test_handler_rfc6455.c"
#endif
