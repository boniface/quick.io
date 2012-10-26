/**
 * Provides a TCP socket connection to an address that is persistent. When there is a
 * disconnect or error, this transparently reconnects on the next socket operation
 * and keeps on going as though nothing went wrong. All sockets used are non-blocking.
 *
 * By its very nature, persistent sockets hide errors, so the functions don't
 * typically return errors. This is essentially a UDP socket that has guaranteed delivery
 * while connected.
 *
 * @file persistent_socket.h
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012 Andrew Stone
 *
 * @internal This file is part of persistent-socket and is released under the
 * MIT License: http://opensource.org/licenses/MIT
 */

/**
 * All of the data needed to maintain a connection to a server.
 */
typedef struct {
	/**
	 * The underlying socket.
	 */
	int socket;
	
	/**
	 * The address info that should be used to reconnect on disconnect.
	 */
	struct addrinfo *addrs;
	
	/**
	 * The current address that we're looking at in the address chain.
	 */
	struct addrinfo *curr_addr;
} psocket;

/**
 * Create a new connection.
 * 
 * @param host The hostname to connect to.
 * @param port The port to connect to.
 * @param[out] psocket Where the new persistent socket should be put.
 *
 * @return 0 on success.
 * @return -1 on address lookup error.
 */
int psocket_connect(char *host, int port, psocket **psocket);

/**
 * Closes a psocket connection.
 *
 * @param psocket The persistent socket to close.
 */
void psocket_close(psocket *psocket);

/**
 * Send some data out to the server.
 *
 * @param psocket The persistent socket
 * @param msg The message to send
 * @param len The length of the message
 */
void psocket_send(psocket *psocket, char *msg, int len);

/**
 * Read data from the server.
 *
 * @param psocket The socket to read from
 * @param buff Where the data should be put
 * @param len The length of the data buffer
 *
 * @return The length of data read from the socket, 0 or greater.
 */
int psocket_read(psocket *psocket, char *buff, size_t len);