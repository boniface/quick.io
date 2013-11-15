/**
 * The official QuickIO protocol over websockets (rfc6455).
 * @file rfc6455.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#ifndef QUICKIO_PROTOCOL_RFC6455_H
#define QUICKIO_PROTOCOL_RFC6455_H

/**
 * If rfc6455 can handle this client
 */
enum protocol_handles protocol_rfc6455_handles(
	struct client *client,
	void **data);

/**
 * To do the rfc6455 handshake
 */
enum protocol_status protocol_rfc6455_handshake(
	struct client *client,
	void *data);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_rfc6455_route(struct client *client);

/**
 * Terminates all communications with the client
 */
void protocol_rfc6455_close(struct client *client, guint reason);

#endif
