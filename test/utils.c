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

void u_main_setup(pid_t *pid) {
	int out[2];
	test_int32_eq(pipe(out), 0, "Pipes ready");
	
	*pid = fork();
	if (*pid) {
		// Parse the config options so they can be used locally
		GError *error = NULL;
		option_parse_config_file(NULL, NULL, 0, &error);
		
		close(out[1]);
		
		// The server needs some time to get started
		sleep(1);
		
		// Wait for the server to emit "READY", then we can run our tests
		char buff[200];
		memset(&buff, 0, sizeof(buff));
		read(out[0], buff, sizeof(buff));
		test_str_eq(buff, "READY\n", "Server inited");
		close(out[0]);
	} else {
		close(out[0]);
		
		// Setup the child's stdout to be readable for the "READY" command
		dup2(out[1], STDOUT_FILENO);
		
		char *args[] = {"."};
		init_main(1, args);
		close(out[1]);
	}
}

void u_main_teardown(pid_t pid) {
	kill(pid, SIGTERM);
}

int u_connect() {
	int sock;
	
	struct sockaddr_in addy;
	memset(&addy, 0, sizeof(addy));
	addy.sin_family = AF_INET;
	addy.sin_addr.s_addr = inet_addr(option_bind_address());
	addy.sin_port = htons(option_port());

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		ERRORF("Could not create socket: %s", strerror(errno));
		return 0;
	}

	if (connect(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not connect: %s", strerror(errno));
		return 0;
	}
	
	return sock;
}