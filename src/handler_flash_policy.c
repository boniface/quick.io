#include "qio.h"

gboolean h_flash_policy_handles(gchar *req) {
	if (*req == '<') {
		return strncmp(req, H_FLASH_POLICY_REQUEST, sizeof(H_FLASH_POLICY_REQUEST)) == 0;
	}
	
	return FALSE;
}

status_t h_flash_policy_handshake(client_t *client) {
	g_string_append(client->message->buffer, H_FLASH_POLICY_RESPONSE);
	
	// While I absolutely abhor putting a write in a handler, it is far cleaner than the
	// alternative: modifying the connections.c message handler to know about flash.
	// From here, we can just write to the client and abort him rather than have to have
	// connections have weird conditions about it.
	// UGHHHHHH
	client_write_frame(client, client->message->buffer->str, client->message->buffer->len);
	
	return CLIENT_ABORTED;
}

#ifdef TESTING
#include "../test/test_handler_flash_policy.c"
#endif