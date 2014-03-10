/**
 * Browsers don't expose HTTP response headers for WebSocket connections,
 * and they don't allow you to set Upgrade or Connection headers for ajax
 * requests, so detecting proxies isn't really possible, so pretty much, with
 * browsers, if they can't connect to QuickIO, but they can see this page,
 * they need to upgrade to TLS.
 * @file post_message.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Sets up everything to run
 */
void protocol_post_message_init();

/**
 * If flash can handle this client
 */
enum protocol_handles protocol_post_message_handles(struct client *client);

/**
 * To complete the single, simple flash handshake.
 */
enum protocol_status protocol_post_message_handshake(struct client *client);
