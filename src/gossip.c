#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

#include "debug.h"
#include "gossip.h"
#include "option.h"

void gossip_client(int pipe) {

}

gboolean gossip_server(int* pipes) {
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
		int buff[1001];
		int bytes = recvfrom(sock, buff, sizeof(buff)-1, 0, NULL, NULL);
		
		// If there's nothing there...w/e
		if (bytes <= 0) {
			continue;
		}
		
		// Null-terminate our command
		buff[bytes] = 0;
		
		DEBUGF("UDP: %s", buff);
	}
}