#include <stdio.h>

#include "pubsub.h"
#include "socket.h"

#include "debug.h"

int main(int argc, char *argv[]) {
	if (g_thread_supported()) {
		g_thread_init(NULL);
	} else {
		ERROR("Threading not supported; refusing to run.");
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
	
	socket_finish();
	
	return 0;
}