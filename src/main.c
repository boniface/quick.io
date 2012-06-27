#include <stdio.h>

#include "commands.h"
#include "debug.h"
#include "option.h"
#include "pubsub.h"
#include "socket.h"

int main(int argc, char *argv[]) {
	if (g_thread_supported()) {
		g_thread_init(NULL);
	} else {
		ERROR("Threading not supported; refusing to run.");
		return 1;
	}
	
	GError *error = NULL;
	if (option_parse_args(argc, argv, error)) {
		DEBUG("Options parsed");
	} else {
		ERROR(error->message);
		return 1;
	}
	
	if (socket_init()) {
		DEBUG("External socket inited");
	} else {
		ERROR("Could not init websocket.");
		return 1;
	}
	
	if (pubsub_init()) {
		DEBUG("Pubsub inited");
	} else {
		ERROR("Could not init pubsub.");
		return 1;
	}
	
	if (commands_init()) {
		DEBUG("Commands inited");
	} else {
		ERROR("Could not init commands.");
		return 1;
	}
	
	printf("READY\n");
	fflush(stdout);
	
	socket_finish();
	
	return 0;
}