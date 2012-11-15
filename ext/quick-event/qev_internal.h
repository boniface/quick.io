/**
 * Defines the functions used INTERNALLY by quick-event
 * @file qev_internal.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of quick-event and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#ifdef QEV_CLIENT_SLOT
	#define QEV_CSLOT(client, slot) client->QEV_CLIENT_SLOT.slot
#else
	/**
	 * Access the qev_client slot of the QEV_CLIENT_T
	 */
	#define QEV_CSLOT(client, slot) client->slot
#endif

#ifndef QEV_TIMERS
	#define QEV_NO_TIMERS
	#define QEV_TIMERS
#endif

/**
 * The part of the bitmask identifying an "listening" client type,
 * meaning that this client accepts connections on its socket.
 */
#define QEV_CMASK_LISTENING 0b00000001

/**
 * The part of the bitmask identifying an SSL client
 */
#define QEV_CMASK_SSL 0b00000010

/**
 * Indicates that a client is still shaking hands
 */
#define QEV_CMASK_SSL_HANDSHAKING 0b00000100

/**
 * Indicates that a client is in the process of being closed
 */
#define QEV_CMASK_CLOSING 0b10000000

/**
 * Convert milliseconds to microseconds.  Just so it's easier to read.
 *
 * @param ms The number of milliseconds to put into microseconds.
 */
#define QEV_MS_TO_USEC(ms) (ms * 1000)

/**
 * Convert milliseconds to nanoseconds.  Just so it's easier to read.
 *
 * @param ms The number of milliseconds to put into nanoseconds.
 */
#define QEV_MS_TO_NSEC(ms) (QEV_MS_TO_USEC(ms) * 1000)

/**
 * Convert seconds to microseconds.  Just so it's easier to read.
 *
 * @param secs The number of seconds to put into microseconds.
 */
#define QEV_SEC_TO_USEC(secs) QEV_MS_TO_USEC(secs * 1000)

/**
 * Listen on a ip address and port (system-specific).
 *
 * @param ip_address The IP address to listen on
 * @param port The port to listen on
 * @param client Where to put the fake client that is created and added to the poll
 *
 * @return The underlying socket that was created.
 */
qev_socket_t qev_sys_listen(char *ip_address, uint16_t port, QEV_CLIENT_T **client);

/**
 * Tell qev that a client is read to be read. This function synchronizes itself
 * across threads.
 *
 * @param client The client ready for reading.
 */
void qev_client_read(QEV_CLIENT_T *client);

/**
 * A notification that a client has been instructed to close: this should clean up all
 * internal structures such that it is safe to free() the client when all threads
 * have synchronized (aka. issued their ticks). This should include removing
 * the client's socket from whatever backend poll is being used, closing the socket,
 * and all that jazz. Once everything is closed up, the socket should also be set
 * to a value such that it is reocgnizable as closed.
 *
 * @param client The client that has been instructed to close.
 */
void qev_sys_client_closed(QEV_CLIENT_T *client);

/**
 * Initializes all system-specific _everythings_ before running.
 * The following should be initialized:
 *   1) All timers, defined in QEV_TIMERS
 *
 * @note Prints to stderr when there is an error
 *
 * @return 0 If everything was setup correctly and quick event can run.
 * @return -1 If there was a fatal error.
 */
int qev_sys_init();

/**
 * Create a new client
 *
 * @return A newly-allocated client.
 */
QEV_CLIENT_T* qev_create_client();