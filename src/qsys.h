/**
 * The abstracted, system-specific functions and defintions that make
 * life a bit simpler.
 * @file qsys.h
 */

#pragma once
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
 * The number of connections the operating system can queue.
 */
#define LISTEN_BACKLOG 1000

/**
 * The amount of time between maintenance ticks (in milliseconds).
 *
 * @attention THIS MUST NEVER BE > 1000.
 */
#define MAINTENANCE_TICK 100

/**
 * The amount of time between hearbeats (in seconds).
 */
#define HEARTBEAT_TICK 60

/**
 * Convert milliseconds to microseconds.  Just so it's easier to read.
 *
 * @param ms The number of milliseconds to put into microseconds.
 */
#define MS_TO_USEC(ms) (ms * 1000)

/**
 * Convert milliseconds to nanoseconds.  Just so it's easier to read.
 *
 * @param ms The number of milliseconds to put into nanoseconds.
 */
#define MS_TO_NSEC(ms) (MS_TO_USEC(ms) * 1000)

/**
 * Convert seconds to microseconds.  Just so it's easier to read.
 *
 * @param secs The number of seconds to put into microseconds.
 */
#define SEC_TO_USEC(secs) MS_TO_USEC(secs * 1000)

/**
 * The socket type, because I don't want to remember it's an int.
 */
typedef int qsys_socket_t;

/**
 * Sets up the socket interface for listening, and runs the main loop of the process.
 */
void qsys_main_loop();

/**
 * Listens on a socket, returns -1 on error.
 *
 * @return -1 if fails, the FD otherwise.
 */
qsys_socket_t qsys_listen(gchar *address, guint16 port);

/**
 * Reads from the client into the buffer.
 *
 * @param client The client to read from
 * @param buff The buffer to fill with data
 * @param buff_len The size of the buffer
 *
 * @return The amount of data read from the client.
 */
gssize qsys_read(client_t *client, gchar *buff, gsize buff_len);

/**
 * Writes a buffer to a client.
 *
 * @param client The client to send to.
 * @param buff The buffer to send to the client.
 * @param buff_len The length of data on the buffer.
 *
 * @return Standard unix stuffs for send.
 * @see http://linux.die.net/man/2/send
 */
gssize qsys_write(client_t *client, gchar *buff, gsize buff_len);

/**
 * Closes a client and removes him from the loop.
 *
 * @param client The client to destroy.
 */
void qsys_close(client_t *client);

#ifdef TESTING
#include "../test/test_qsys.h"
#endif