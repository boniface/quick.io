/**
 * Handles sending the flash socket policy file back on flash connections.
 * You're welcome, Internet Explorer.
 * @file flash.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once
#include "quickio.h"

/**
 * Sets up everything to run
 */
void protocol_flash_init();

/**
 * If flash can handle this client
 */
enum protocol_handles protocol_flash_handles(struct client *client);

/**
 * To complete the single, simple flash handshake.
 */
enum protocol_status protocol_flash_handshake(struct client *client);

/**
 * Routes the data received from a client
 */
enum protocol_status protocol_flash_route(struct client *client);
