/**
 * The abstracted, system-specific functions and defintions that make
 * life a bit simpler.
 * @file qsys.h
 */

#pragma once
#include "qio.h"

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
 * Functions that MUST be implemented by system-specific functions.
 * 
 * @defgroup SysSpecific System-specific types and functions
 *
 * These functions are system-specific; as such, they must abstract away all
 * system-related peculiarities.
 */

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
 * The system-specific socket type.  Feel free to overwrite this if it is incorrect.
 * @ingroup SysSpecific
 */
// typedef int qsys_socket;

#include "qsys/epoll.h"

/**
 * Sets up the socket for listening, does all the binding, and everything.
 */
gboolean qsys_listen();

/**
 * Initializes any control structures needed.
 */
gboolean qsys_init();

/**
 * Dispatches 1 round on the event loop.
 */
void qsys_dispatch();

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

/**
 * Initialize any internal data structures necessary for the event loop.
 *
 * @ingroup SysSpecific
 */
gboolean _qsys_init();

/**
 * Accepts a client from the raw socket and fires the conns_client_new() event.
 * 
 * @ingroup SysSpecific
 *
 * @return The sys-specific socket that will be operated on.
 */
void _qsys_accept();

/**
 * Sets up the socket for listening.  This should also set up everything that
 * is needed in order to accept connections.
 *
 * For example, with epoll, this adds the socket to the epoll interface with a
 * dummy client so that we know when there are connections waiting to be accepted.
 *
 * @ingroup SysSpecific
 *
 * @param address The address to listen on.
 * @param port The port to listen on.
 *
 * @return If the socket is being listened on
 */
gboolean _qsys_listen(gchar *address, guint16 port);

/**
 * Runs 1 sys-call for the event loop, the main loop is run in qsys_run().
 * This function MUST dispatch the individual events back to conns_*.
 *
 * @ingroup SysSpecific
 */
void _qsys_dispatch();

/**
 * Reads from the socket into the buffer.
 *
 * @ingroup SysSpecific
 *
 * @note Adheres to standard unix socket return values.
 * @see http://linux.die.net/man/2/read
 */
gssize _qsys_read(qsys_socket socket, gchar *buff, gsize buff_len);

/**
 * Writes a buffer to a client.
 *
 * @ingroup SysSpecific
 *
 * @note Adheres to standard unix socket return values.
 * @see http://linux.die.net/man/2/send
 */
gssize _qsys_write(qsys_socket socket, gchar *buff, gsize buff_len);

/**
 * Closes a socket.  The socket MUST be removed from the event loop.
 *
 * @ingroup SysSpecific
 *
 * @param socket The socket to close.
 */
void _qsys_close(qsys_socket socket);

/**
 * Sets up the maintenance timer.  This MUST be fired every MAINTENANCE_TICK.
 *
 * @ingroup SysSpecific
 *
 * @return If the setup succeeded.
 */
gboolean _qsys_init_maintenance();

#ifdef TESTING
#include "../test/test_qsys.h"
#endif