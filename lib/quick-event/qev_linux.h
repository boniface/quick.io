/**
 * Linux-specific definitions
 * @file qev_linux.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pwd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * The amount of time to wait in epoll before giving up.
 * This is necessary so that the close queue can be processed: if there aren't many events,
 * the same thread will continually get all the events, and the others will block indefinitely,
 * thus allowing the close queue to grow unbounded.
 */
#define QEV_EPOLL_TIMEOUT 100

/**
 * Sockets on Linux are ints.
 */
typedef int qev_socket_t;