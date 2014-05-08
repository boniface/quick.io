/**
 * Handles sending the flash socket policy file back on flash connections.
 * You're welcome, Internet Explorer.
 * @file protocols_flash.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2014 Clear Channel Inc.
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://opensource.org/licenses/MIT
 */

#pragma once
#include "quickio.h"

/**
 * This protocol's functions.
 */
struct protocol *protocol_flash;

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
