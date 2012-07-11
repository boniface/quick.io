#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"
#include "gossip.h"
#include "option.h"

// The pipes for communicating with the parent
static gossip_t *_gossip;

gboolean gossip_client() {
	// GError *error = NULL;
	// GThread *_thread = g_thread_try_new(__FILE__, _gossip_client, NULL, &error);
	// if (_thread == NULL) {
	// 	ERRORF("Could init gossip thread: %s", error->message);
	// 	return FALSE;
	// }
	
	return TRUE;
}

gboolean gossip_server() {
	int sock;
	struct sockaddr_in addy;
	
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		ERROR("Could not create socket");
		return FALSE;
	}
	
	addy.sin_family = AF_INET;
	addy.sin_port = htons(option_gossip_port());
	addy.sin_addr.s_addr = inet_addr(option_gossip_address());
	memset(&addy.sin_zero, 0, sizeof(addy.sin_zero));
	
	int on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
		ERROR("Could not set socket option");
		return FALSE;
	}
	
	if (bind(sock, (struct sockaddr*)&addy, sizeof(addy)) == -1) {
		ERRORF("Could not bind: %s", strerror(errno));
		return FALSE;
	}
	
	while (1) {
		// char buff[1001];
		// int bytes = recvfrom(sock, buff, sizeof(buff)-1, 0, NULL, NULL);
		
		// // If there's nothing there...w/e
		// if (bytes <= 0) {
		// 	continue;
		// }
		
		// // Null-terminate our command
		// buff[bytes] = 0;
		
		// DEBUGF("UDP: %s", buff);
		
		sleep(2);
		guint32 conns = __sync_fetch_and_and(&_gossip->connects, 0);
		DEBUGF("Clients: %d; Connections: %d", _gossip->clients, conns);
	}
}

void gossip_client_connect() {
	__sync_add_and_fetch(&_gossip->connects, 1);
	__sync_add_and_fetch(&_gossip->clients, 1);
}

void gossip_client_disconnect() {
	__sync_sub_and_fetch(&_gossip->clients, 1);
}

gboolean gossip_init() {
	// Step 1: We need memory!
	int stats = shm_open(SHM_NAME, SHM_FLAGS, SHM_MASK);
	if (stats == -1) {
		ERRORF("Gossip SHM error: %s", strerror(errno));
		return FALSE;
	}
	
	// Step 2: Set the memory size
	if (ftruncate(stats, sizeof(*_gossip)) == -1) {
		ERRORF("Gossip truncate error: %s", strerror(errno));
		return FALSE;
	}
	
	// Step 3: Get the shared memory mapped into us
	_gossip = mmap(NULL, sizeof(*_gossip), PROT_READ | PROT_WRITE, MAP_SHARED, stats, 0);
	
	if (*((int*)_gossip) == -1) {
		ERRORF("Gossip mmap error: %s", strerror(errno));
		return FALSE;
	}
	
	_gossip->clients = 0;
	_gossip->connects = 0;
	
	return TRUE;
}