#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "handler_rfc6455.h"
#include "handler_rfc6455_const.h"

#include "debug.h"

/**
 * The corresponding RFC: http://tools.ietf.org/html/rfc6455
 */

gboolean rfc6455_handles(SoupMessageHeaders *req_headers) {
	const char *id = soup_message_headers_get_one(req_headers, VERSION_KEY);
	return id != NULL && strncmp(id, "13", 2) == 0;
}

gboolean rfc6455_handshake(const int sock, SoupMessageHeaders *req_headers) {
	const char *key = soup_message_headers_get_one(req_headers, CHALLENGE_KEY);
	
	if (key == NULL) {
		return FALSE;
	}
	
	//build up the concated key, ready for hashing for the return header
	//-1 to kill the null terminator from HASH_KEY
	guchar out[strlen(key) + HASH_KEY_LEN];
	strcpy((char*)out, key);
	strcat((char*)out, HASH_KEY);
	
	gsize size = strlen((char*)out);
	GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA1);
	
	//do some hard-core hashing, then free it all
	g_checksum_update(sum, out, size);
	g_checksum_get_digest(sum, out, &size);
	gchar *b64 = g_base64_encode(out, size);
	g_checksum_free(sum);
	
	size = HEADERS_LEN + strlen(b64);
	char headers[size];
	
	snprintf(headers, size, HEADERS, b64);
	DEBUG(headers);
	write(sock, headers, strlen(headers));
	
	g_free(b64);
	
	return TRUE;
}

char* rfc6455_prepare_frame(char *msg, int *frame_len) {
	char *frame;
	
	// If 125 chars or less, then only use 7 bits to represent
	// payload length
	//
	// If greater than 125, then the first 7 bits should be 126,
	// and the following two bytes will be interpreted as a
	// 16-bit unsigned int
	int payload_len = strlen(msg);
	if (payload_len <= 125) {
		// Since this is a small message, we only need 2 extra bytes
		// to represent the data
		*frame_len = (payload_len + 2);
		
		frame = malloc(*frame_len * sizeof(char));
		
		// The first frame of text is ALWAYS the same
		*frame = FIRST_BYTE_TEXT;
		
		// The second frame: the first bit is always 0 (data going
		// to the client is never masked), and since the length is
		// less than 125, that bit will ALWAYS be set to 0, as it should
		*(frame + 1) = payload_len;
		
		memcpy((frame + 2), msg, payload_len);
	} else {
		ERROR("Msg len > 125, not implemented");
		*frame_len = -1;
		return NULL;
	}
	
	return frame;
}