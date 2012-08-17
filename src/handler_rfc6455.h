/**
 * Handle clients that adhere to rfc6455.
 * @see http://tools.ietf.org/html/rfc6455
 * @file handler_rfc6455.h
 */

#pragma once
#include "qio.h"

/**
 * If, given the headers, we support this client.
 *
 * @param path The path that came from the request.
 * @param headers The HTTP headers the client sent.
 *
 * @return If this handler can handle this client.
 */
gboolean rfc6455_handles(gchar *path, SoupMessageHeaders *headers);

/**
 * Handshake with the client.
 *
 * @param client The client to handshake with
 * @param headers The HTTP headers the client sent.
 *
 * @return If the handshake succeeded.
 */
status_t rfc6455_handshake(client_t *client, SoupMessageHeaders *headers);

/**
 * Prepare a frame to be sent to the client.
 *
 * @param type The opcode to put into the frame.
 * @param masked If the frame should be masked.
 * @param payload The data to be put into the response
 * @param payload_len The length of the payload
 * @param[out] frame_len The length of the final frame.
 *
 * @return The fully-framed frame to write to the client. If NULL, there was an error in 
 * frame creation.
 * @attention The return of this function MUST be free'd
 */
char* rfc6455_prepare_frame(opcode_t type, gboolean masked, gchar *payload, guint64 payload_len, gsize *frame_len);

/**
 * An accessor for rfc6455_prepare_frame.
 *
 * @see rfc6455_prepare_frame
 */
char* rfc6455_prepare_frame_from_message(message_t *message, gsize *frame_len);

/**
 * Read incoming data from the client.
 * Takes the incoming data, de-masks it, and adds it to the message buffer.
 *
 * @param client The client sending data.
 * 
 * @return Any client status.
 * @return CLIENT_WAIT indicates that there wasn't enough data in the socket to create a message.
 */
status_t rfc6455_incoming(client_t *client);

/**
 * After a CLIENT_WAIT and remaining_length are set, this will be called to
 * continue/finish reading a broken message.
 *
 * @param client The client sending data.
 */
status_t rfc6455_continue(client_t *client);

#ifdef TESTING
#include "../test/test_handler_rfc6455.h"
#endif