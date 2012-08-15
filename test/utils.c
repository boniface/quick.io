#include "test.h"

char* test_status_to_str(status_t status) {
	switch (status) {
		#include "client_statuses.inc"
	}
	return "UNKNOWN";
}

client_t* u_client_create() {
	client_t *client = malloc(sizeof(*client));
	memset(client, 0, sizeof(*client));
	
	message_t *message = malloc(sizeof(*message));
	memset(message, 0, sizeof(*message));
	
	message->socket_buffer = g_string_sized_new(1);
	message->buffer = g_string_sized_new(1);
	
	client->subs = g_ptr_array_new();
	
	client->message = message;
	client->handler = h_none;
	
	return client;
}

void u_client_free(client_t *client) {
	g_string_free(client->message->socket_buffer, TRUE);
	g_string_free(client->message->buffer, TRUE);
	g_ptr_array_free(client->subs, TRUE);
	free(client->message);
	free(client);
}