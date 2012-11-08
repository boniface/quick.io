/**
 * persistent_socket.c
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

#include "persistent_socket.h"

/**
 * Attempt to connect again, looping through all the addresses available until
 * one works.
 */
static void _connect(psocket *psocket) {
	if (psocket->socket != -1) {
		close(psocket->socket);
		psocket->socket = -1;
	}
	
	psocket->curr_addr = psocket->curr_addr->ai_next;
	
	// At the end of the chain, loop back to the beginning
	if (psocket->curr_addr == NULL) {
		psocket->curr_addr = psocket->addrs;
	}
	
	int sock;
	if ((sock = socket(psocket->curr_addr->ai_family, psocket->curr_addr->ai_socktype, 0)) == -1) {
		return;
	}
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		close(sock);
		return;
	}
	
	if (connect(sock, psocket->curr_addr->ai_addr, psocket->curr_addr->ai_addrlen) == -1 && errno != EINPROGRESS) {
		close(sock);
		return;
	}
	
	// Detect a socket failure earlier -> set the socket buffer smaller to force the kernel
	// to flush and send an error rather than be hopeful.
	//
	// This addresses the case:
	//   1) A connecction is opened, and data is sent
	//   2) A firewall in the middle drops the connection, no HUPs sent
	//   3) The kernel faithfully tries to resend the data, but to no avail, until the OS's default buffer fills
	//   4) We FINALLY get the notification of the error
	//
	// Here, set the buffer size to be that the max size of a UDP packet: this allows us to detect errors even faster
	// (See: http://stackoverflow.com/questions/1098897/what-is-the-largest-safe-udp-packet-size-on-the-internet)
	int buff_len = 512;
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &buff_len, sizeof(&buff_len)) == -1) {
		close(sock);
		return;
	}
	
	psocket->socket = sock;
}

int psocket_connect(char *host, int port, psocket **psocket) {
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
	
	*psocket = malloc(sizeof(*psocket));
	(*psocket)->socket = -1;
	(*psocket)->addrs = (*psocket)->curr_addr = res;
	
	_connect(*psocket);
	
	return 0;
}

void psocket_close(psocket *psocket) {
	close(psocket->socket);
	freeaddrinfo(psocket->addrs);
	free(psocket);
}

void psocket_send(psocket *psocket, char *msg, int len) {
	if (send(psocket->socket, msg, len, MSG_NOSIGNAL) != len) {
		_connect(psocket);
	}
}

int psocket_read(psocket *psocket, char *buff, size_t len) {
	int got = recv(psocket->socket, buff, len, 0);
	
	if (got <= 0 && errno != EAGAIN) {
		_connect(psocket);
		return 0;
	}
	
	return got < 0 ? 0 : got;
}