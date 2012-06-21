#include <stdio.h>

#include "accepter.h"
#include "websocket.h"

#include "debug.h"

int main(int argc, char *argv[]) {
	if (g_thread_supported()) {
		g_thread_init(NULL);
	} else {
		ERROR("Threading not supported; refusing to run.");
		return 1;
	}
	
	if (accepter_init()) {
		DEBUG("Accepter inited");
	} else {
		ERROR("Could not init accepter.");
		return 1;
	}
	
	if (ws_init()) {
		DEBUG("Websocket inited");
	} else {
		ERROR("Could not init websocket.");
		return 1;
	}
	
	accepter_finish();
	
	return 0;
}