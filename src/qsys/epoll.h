#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "qio.h"

/**
 * Events epoll should wait for.
 */
#define EPOLL_READ_EVENTS EPOLLIN | EPOLLRDHUP | EPOLLET

/**
 * The maximum number of events to return per epoll round.
 */
#define EPOLL_MAX_EVENTS 100

/**
 * How long the epoll should wait for events.
 * -1 just to wait forever until something happens.
 */
#define EPOLL_WAIT -1

/**
 * The socket type for the system.
 */
typedef int qsys_socket;