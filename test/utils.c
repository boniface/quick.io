#include "test.h"

char* test_status_to_str(status_t status) {
	switch (status) {
		case 1 << 0: return "CLIENT_GOOD";
		case 1 << 1: return "CLIENT_FATAL";
		case 1 << 2: return "CLIENT_ERROR";
		case 1 << 3: return "CLIENT_ASYNC";
		case 1 << 4: return "CLIENT_WRITE";
		case 1 << 5: return "CLIENT_WAIT";
	}
	return "UNKNOWN";
}

client_t* u_client_create(int *socket) {
	client_t *client = malloc(sizeof(*client));
	memset(client, 0, sizeof(*client));
	
	message_t *message = malloc(sizeof(*message));
	memset(message, 0, sizeof(*message));
	
	message->socket_buffer = g_string_sized_new(1);
	message->buffer = g_string_sized_new(1);
	
	client->subs = g_ptr_array_new();
	
	client->message = message;
	client->handler = h_none;
	client->last_receive = qev_time;
	
	if (socket != NULL) {
		int sockets[2];
		socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
		client->qevclient.socket = sockets[0];
		*socket = sockets[1];
	} else {
		client->qevclient.socket = -1;
	}
	
	client_ref(client);
	
	return client;
}

void u_client_free(client_t *client) {
	if (client->qevclient.socket != 0) {
		close(client->qevclient.socket);
	}

	g_string_free(client->message->socket_buffer, TRUE);
	g_string_free(client->message->buffer, TRUE);
	g_ptr_array_free(client->subs, TRUE);
	free(client->message);
	free(client);
}

void u_main_setup(pid_t *pid) {
	int out[2];
	test_int32_eq(pipe(out), 0, "Pipes ready");
	
	utils_stats_setup();
	
	*pid = fork();
	if (*pid) {
		// Parse the config options so they can be used locally
		GError *error = NULL;
		option_parse_args(0, NULL, NULL);
		option_parse_config_file(NULL, NULL, 0, &error);
		
		close(out[1]);
		
		for (int i = 0; i < 5; i++) {
			// The server needs some time to get started
			usleep(MS_TO_USEC(20));
			
			// Wait for the server to emit "QIO READY", then we can run our tests
			char buff[11];
			memset(&buff, 0, sizeof(buff));
			if (read(out[0], buff, sizeof(buff)) > 0) {
				DEBUG("%s", buff);
				test_str_eq(buff, "QIO READY\n", "Server inited");
			}
			
			close(out[0]);
			
			return;
		}
		
		test(FALSE, "Server failed to init");
	} else {
		close(out[0]);
		
		// Setup the child's stdout to be readable for the "QIO READY" command
		dup2(out[1], STDOUT_FILENO);
		
		char *args[] = {"."};
		init_main(1, args);
		close(out[1]);
	}
}

void u_main_teardown(pid_t pid) {
	kill(pid, SIGTERM);
	utils_stats_teardown();
}

int u_connect() {
	int sock;
	
	struct sockaddr_in addy;
	memset(&addy, 0, sizeof(addy));
	addy.sin_family = AF_INET;
	addy.sin_addr.s_addr = inet_addr(option_bind_address());
	addy.sin_port = htons(option_bind_port());

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		CRITICAL("Could not create socket: %s", strerror(errno));
		return 0;
	}

	if (connect(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		CRITICAL("Could not connect: %s", strerror(errno));
		return 0;
	}
	
	return sock;
}

int u_ws_connect() {
	int sock = u_connect();
	
	char buff[sizeof(U_HANDSHAKE_RESPONSE)];
	memset(&buff, 0, sizeof(buff));
	
	test(sock, "Connection established");
	test_int32_eq(send(sock, U_HANDSHAKE, sizeof(U_HANDSHAKE)-1, 0), sizeof(U_HANDSHAKE)-1, "Handshake sent");
	test_int32_eq(read(sock, buff, sizeof(buff)), sizeof(U_HANDSHAKE_RESPONSE)-1, "Correct response length read");
	
	test_str_eq(buff, U_HANDSHAKE_RESPONSE, "Correct response sent");
	
	return sock;
}
