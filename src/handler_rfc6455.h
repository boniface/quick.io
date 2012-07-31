#pragma once
#include "qio.h"

/**
 * If, given the headers, we support this client.
 */
gboolean rfc6455_handles(gchar*, SoupMessageHeaders*);

/**
 * Handshake with the client.
 */
gboolean rfc6455_handshake(client_t*, SoupMessageHeaders*);

/**
 * Prepare a frame to be sent to the client.
 *
 * The return of the function must be free()'d
 */
char* rfc6455_prepare_frame(opcode_t, gchar*, int, int*);

/**
 * Prepare a frame from a client message.
 */
char* rfc6455_prepare_frame_from_message(message_t*, int*);

/**
 * Read an incoming from from the client.
 *
 * If returns true, client->buffer will contain the message/command recieved
 * from the client.
 *
 * If returns false, more data from the client is necessary to construct the message.
 */
status_t rfc6455_incoming(client_t*);

/**
 * After a CLIENT_WAIT and remaining_length are set, this will be called to continue/finish
 * reading a broken message.
 */
status_t rfc6455_continue(client_t*);