#include "test.h"

client_t* u_client_create() {
	client_t *client = malloc(sizeof(*client));
	memset(client, 0, sizeof(*client));
	
	message_t *message = malloc(sizeof(*message));
	memset(message, 0, sizeof(*message));
	
	message->socket_buffer = g_string_sized_new(1);
	message->buffer = g_string_sized_new(1);
	
	client->message = message;
	
	return client;
}

void u_client_free(client_t *client) {
	g_string_free(client->message->socket_buffer, TRUE);
	g_string_free(client->message->buffer, TRUE);
	free(client->message);
	free(client);
}