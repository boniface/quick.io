#include "qio.h"

gboolean h_flash_policy_handles(gchar *req) {
	if (*req == '<') {
		return strncmp(req, H_FLASH_POLICY_REQUEST, sizeof(H_FLASH_POLICY_REQUEST)) == 0;
	}
	
	return FALSE;
}

status_t h_flash_policy_handshake(client_t *client) {
	g_string_append(client->message->buffer, H_FLASH_POLICY_RESPONSE);
	return CLIENT_WRITE | CLIENT_ABORTED;
}

#ifdef TESTING
#include "../test/test_handler_flash_policy.c"
#endif