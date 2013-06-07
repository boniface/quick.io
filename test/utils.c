#include "test.h"

char* test_status_to_str(enum status status)
{
	switch (status) {
		case CLIENT_GOOD: return "CLIENT_GOOD";
		case CLIENT_FATAL: return "CLIENT_FATAL";
		case CLIENT_ERROR: return "CLIENT_ERROR";
		case CLIENT_ASYNC: return "CLIENT_ASYNC";
		case CLIENT_WRITE: return "CLIENT_WRITE";
		case CLIENT_WAIT: return "CLIENT_WAIT";
	}
	return "UNKNOWN";
}

struct client* u_client_create(int *socket)
{
	struct client *client = g_slice_alloc0(sizeof(*client));
	struct message *message = g_slice_alloc0(sizeof(*message));

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

void u_client_free(struct client *client)
{
	if (client->qevclient.socket != 0) {
		close(client->qevclient.socket);
	}

	if (client->message != NULL) {
		g_string_free(client->message->socket_buffer, TRUE);
		g_string_free(client->message->buffer, TRUE);
	}

	if (client->subs != NULL) {
		g_ptr_array_free(client->subs, TRUE);
	}

	g_slice_free1(sizeof(*client->message), client->message);
	g_slice_free1(sizeof(*client), client);
}

void u_main_setup(pid_t *pid, gchar* config_path)
{
	int out[2];
	check_int32_eq(pipe(out), 0, "Pipes ready");

	utils_stats_setup();

	*pid = fork();
	if (*pid) {
		// Parse the config options so they can be used locally
		option_parse_args(0, NULL, NULL);
		option_parse_config_file(NULL, NULL, 0);

		close(out[1]);

		for (int i = 0; i < 5; i++) {
			// The server needs some time to get started
			usleep(MS_TO_USEC(20));

			// Wait for the server to emit "QIO READY", then we can run our tests
			char buff[11];
			memset(&buff, 0, sizeof(buff));
			if (read(out[0], buff, sizeof(buff)) > 0) {
				DEBUG("%s", buff);
				check_str_eq(buff, "QIO READY\n", "Server inited");
			}

			close(out[0]);

			return;
		}

		check(FALSE, "Server failed to init");
	} else {
		close(out[0]);

		// Setup the child's stdout to be readable for the "QIO READY" command
		dup2(out[1], STDOUT_FILENO);

		int argc;
		char **argv;
		if (config_path == NULL) {
			gchar *_argv[] = {"."};
			argv = _argv;
			argc = 1;
		} else {
			gchar *_argv[] = {".", "-c", config_path};
			argv = _argv;
			argc = 3;
		}

		exit(init_main(argc, argv));
	}
}

void u_main_teardown(pid_t pid)
{
	kill(pid, SIGTERM);
	utils_stats_teardown();
}

int u_connect()
{
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

int u_ws_connect()
{
	int sock = u_connect();

	char buff[sizeof(U_HANDSHAKE_RESPONSE)];
	memset(&buff, 0, sizeof(buff));

	check(sock, "Connection established");
	check_int32_eq(send(sock, U_HANDSHAKE, sizeof(U_HANDSHAKE)-1, 0), sizeof(U_HANDSHAKE)-1, "Handshake sent");
	check_int32_eq(read(sock, buff, sizeof(buff)), sizeof(U_HANDSHAKE_RESPONSE)-1, "Correct response length read");

	check_str_eq(buff, U_HANDSHAKE_RESPONSE, "Correct response sent");

	return sock;
}
