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
 * @param ms The number of milliseconds to put into nanoseconds.
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
 * @param secs The number of seconds to put into nanoseconds.
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
 * Runs the accept loop.  This MUST be run from a separate thread.
 *
 * @param not_used This is not used in the thread.
 *
 * @return Nothing will ever be returned.
 */
gpointer qsys_accept(gpointer not_used);

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
 * Sets a timer on a client.
 *
 * @param client The client to set a timer on.
 * @param sec The number of seconds to wait
 * @param ms The number of milliseconds to wait.
 *
 * @return If the timer create suceeded.  If FALSE, the client should be closed.
 */
gboolean qsys_timer_set(client_t *client, guint16 sec, guint16 ms);

/**
 * Clears the timer on a client.
 */
void qsys_timer_clear(client_t *client);

/**
 * Initialize any internal data structures necessary for the event loop.
 *
 * @ingroup SysSpecific
 */
gboolean _qsys_init();

/**
 * Accepts a client from the raw socket.
 * 
 * @ingroup SysSpecific
 *
 * @param socket The socket to accept the client on.
 *
 * @return The sys-specific socket that will be operated on.
 */
client_t* _qsys_accept(qsys_socket socket);

/**
 * Sets up the socket for listening.
 *
 * @ingroup SysSpecific
 *
 * @note You should print out errors as they occur and return 0/ NULL if
 * something fails.
 *
 * @param address The address to listen on.
 * @param port The port to listen on.
 *
 * @return The socket that is ready for listening, or 0/NULL if there's a failure.
 */
qsys_socket _qsys_listen(gchar *address, guint16 port);

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
 * Sets a timer on a client and adds the client timer to the event loop for
 * notification.
 *
 * @ingroup SysSpecific
 *
 * @param client The client to be notified on timer.
 * @param sec The number of seconds to wait
 * @param ms The number of milliseconds to wait.
 *
 * @return 0/NULL if the add failed.
 */
gboolean _qsys_timer_set(client_t *client, guint16 sec, guint16 ms);

/**
 * Clears the timer and removes it from the event loop.
 *
 * @ingroup SysSpecific
 *
 * @param client The client to clean up.
 */
void _qsys_timer_clear(client_t *client);

#ifdef TESTING
#include "../test/test_qsys.h"
#endif