/**
 * Utilities for running test cases.
 * @file utils.h
 */

#pragma once
#include "test.h"

/**
 * A typical, RFC6455 handshake.
 */
#define U_HANDSHAKE "GET /qio HTTP/1.1\r\n" \
	"Host: server.example.com\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" \
	"Origin: http://example.com\r\n" \
	"Sec-WebSocket-Version: 13\r\n\r\n"

/**
 * What the server MUST responsd with, to the handshake above.
 */
#define U_HANDSHAKE_RESPONSE "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n"

/**
 * Convert a status_t to a string, for printing.
 *
 * @param status The status to look for.
 *
 * @return A printable string that names the status, or NULL if not found / multiple statuses.
 */
char* test_status_to_str(status_t status);

/**
 * Create a new client for tests.
 *
 * @param socket Where the other side of the client socket should be put
 *
 * @return A client that can be used.
 */
client_t* u_client_create(int *socket);

/**
 * Free the client allocated in u_client_create().
 *
 * @param client The client to free.
 */
void u_client_free(client_t *client);

/**
 * Setup the server and get it running.  Returns once the server is ready to function.
 *
 * @param[out] pid The pid of the running server.
 */
void u_main_setup(pid_t *pid);

/**
 * Kill the server and all its children.
 *
 * @param pid The pid of the server to murder.
 */
void u_main_teardown(pid_t pid);

/**
 * Connect to a running server instance.
 *
 * @return The socket connected to the server.
 */
int u_connect();

/**
 * Connect to a running server instance, and complete an RFC6455 handshake.
 *
 * @return The socket connected to the server.
 */
int u_ws_connect();
