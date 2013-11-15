/**
 * Support for stomp's pub/sub.
 * @file stomp.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#ifndef QUICKIO_PROTOCOL_STOMP_H
#define QUICKIO_PROTOCOL_STOMP_H

/**
 * If stomp can handle this client
 */
enum protocol_handles protocol_stomp_handles(struct client *client, void **data);

/**
 * To do the stomp handshake
 */
enum protocol_status protocol_stomp_handshake(struct client *client, void *data);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_stomp_route(struct client *client);

#endif