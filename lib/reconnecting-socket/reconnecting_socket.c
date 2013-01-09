/**
 * reconnecting_socket.c
 * Copyright 2012 Andrew Stone <andrew@clovar.com>
 *
 * This file is part of persistent-socket and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "reconnecting_socket.h"

/**
 * Attempt to connect to again, looping through all the addresses available until
 * one works.
 */
static void _connect(rsocket *rsocket) {
	if (rsocket->socket != -1) {
		close(rsocket->socket);
		rsocket->socket = -1;
	}
	
	rsocket->curr_addr = rsocket->curr_addr->ai_next;
	
	// At the end of the chain, loop back to the beginning
	if (rsocket->curr_addr == NULL) {
		rsocket->curr_addr = rsocket->addrs;
	}
	
	int sock;
	if ((sock = socket(rsocket->curr_addr->ai_family, rsocket->curr_addr->ai_socktype, 0)) == -1) {
		return;
	}
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		close(sock);
		return;
	}
	
	if (connect(sock, rsocket->curr_addr->ai_addr, rsocket->curr_addr->ai_addrlen) == -1 && errno != EINPROGRESS) {
		close(sock);
		return;
	}
	
	rsocket->socket = sock;
}

int rsocket_connect(const char *host, const int port, rsocket **rsocket) {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	
	char cport[6];
	snprintf(cport, sizeof(cport), "%d", port);
	
	struct addrinfo *res;
	if (getaddrinfo(host, cport, &hints, &res) != 0) {
		return -1;
	}
	
	*rsocket = malloc(sizeof(**rsocket));
	(*rsocket)->socket = -1;
	(*rsocket)->addrs = (*rsocket)->curr_addr = res;
	
	_connect(*rsocket);
	
	return 0;
}

void rsocket_close(rsocket *rsocket) {
	close(rsocket->socket);
	freeaddrinfo(rsocket->addrs);
	free(rsocket);
}

void rsocket_send(rsocket *rsocket, char *msg, int len) {
	if (send(rsocket->socket, msg, len, MSG_NOSIGNAL) != len) {
		_connect(rsocket);
	}
}

int rsocket_read(rsocket *rsocket, char *buff, size_t len) {
	int got = recv(rsocket->socket, buff, len, 0);
	
	if (got <= 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		_connect(rsocket);
		return 0;
	}
	
	return got;
}