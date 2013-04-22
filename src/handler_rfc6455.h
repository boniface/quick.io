/**
 * Handle clients that adhere to rfc6455.
 * @see http://tools.ietf.org/html/rfc6455
 * @file handler_rfc6455.h
 */
#ifndef QIO_HANDLER_RFC6455_H
#define QIO_HANDLER_RFC6455_H

#include "qio.h"

/**
 * Base response headers that can be extended for more options
 */
#define RFC6455_HEADERS "HTTP/1.1 101 Switching Protocols\r\n" \
	"Upgrade: websocket\r\n" \
	"Connection: Upgrade\r\n" \
	"Access-Control-Allow-Origin: *\r\n" \
	"Sec-WebSocket-Accept: %s\r\n"

/**
 * Reads the header off the data and populates all message information.
 *
 * @param client The client to read from
 * @param[out] header_len The length of the header. This is only set on CLIENT_GOOD.
 */
status_t h_rfc6455_read_header(client_t *client, int *header_len);

/**
 * Reads the header off the data and populates all message information.
 */
status_t h_rfc6455_read(client_t *client, int header_len);

/**
 * If, given the headers, we support this client.
 *
 * @param path The path that came from the request.
 * @param headers The HTTP headers the client sent.
 *
 * @return Anything !=-1 if it handles; ==-1 if it doesn't (in reality, these are flags that
 * go to h_rfc6455_handshake)
 */
int h_rfc6455_handles(gchar *path, GHashTable *headers);

/**
 * Gets the RFC6455 Sec-WebSocket-Key value for the client.
 *
 * @param client The client to handshake with.
 * @param headers The HTTP headers the client sent.
 *
 * @return The key for Sec-WebSocket-Key. This MUST be freed.  NULL on failure.
 */
gchar* h_rfc6455_header_key(client_t *client, GHashTable *headers);

/**
 * Handshake with the client.
 *
 * @note client->message->buffer WILL contain the original headers string; once
 * you write to this, you MAY NOT access headers any more as they WILL be invalid.
 *
 * @param client The client to handshake with.
 * @param flags The return value of h_rfc6455_handles
 * @param headers The HTTP headers the client sent.
 *
 * @return If the handshake succeeded.
 */
status_t h_rfc6455_handshake(client_t *client, int flags, GHashTable *headers);

/**
 * Prepare a frame to be sent to the client.
 *
 * @param broadcast If the frame being sent is intended for broadcast.
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
gchar* h_rfc6455_prepare_frame(const gboolean broadcast, const opcode_t type, const gboolean masked, const gchar *payload, const guint64 payload_len, gsize *frame_len);

/**
 * An accessor for h_rfc6455_prepare_frame.
 *
 * @see h_rfc6455_prepare_frame
 */
gchar* h_rfc6455_prepare_frame_from_message(message_t *message, gsize *frame_len);

/**
 * Read incoming data from the client.
 * Takes the incoming data, de-masks it, and adds it to the message buffer.
 *
 * @param client The client sending data.
 *
 * @return Any client status.
 * @return CLIENT_WAIT indicates that there wasn't enough data in the socket to create a message.
 */
status_t h_rfc6455_incoming(client_t *client);

/**
 * After a CLIENT_WAIT and remaining_length are set, this will be called to
 * continue/finish reading a broken message.
 *
 * @param client The client sending data.
 */
status_t h_rfc6455_continue(client_t *client);

/**
 * The frame to write to a client when it closes.
 *
 * @param frame_len The length of the close frame.
 *
 * @return A statically-allocated frame: this must NOT be freed.
 */
gchar* h_rfc6455_close_frame(gsize *frame_len);

#ifdef TESTING
#include "../test/test_handler_rfc6455.h"
#endif

#endif
