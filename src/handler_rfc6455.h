#pragma once
#include <libsoup/soup.h>

#include "client.h"
#include "socket.h"

/**
 * If, given the headers, we support this client.
 */
gboolean rfc6455_handles(SoupMessageHeaders*);

/**
 * Handshake with the client.
 */
gboolean rfc6455_handshake(client_t*, SoupMessageHeaders*);

/**
 * Prepare a frame to be sent to the client.
 *
 * The return of the function must be free()'d
 */
char* rfc6455_prepare_frame(message_t*, int*);

/**
 * Read an incoming from from the client.
 *
 * If returns true, client->buffer will contain the message/command recieved
 * from the client.
 *
 * If returns false, more data from the client is necessary to construct the message.
 */
status_t rfc6455_incoming(client_t*);