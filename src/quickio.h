/**
 * Every header, definition, and whatnot QuickIO needs to run.
 * @file quickio.h
 *
 * @author Andrew Stone <andrew@clovar.com>
 * @copyright 2012-2013 iHeartRadio
 *
 * @internal This file is part of QuickIO and is released under
 * the MIT License: http://www.opensource.org/licenses/mit-license.php
 */

#pragma once

#define G_LOG_DOMAIN "quickio"
#define QEV_CLIENT client

#include <glib.h>
#include "../include/quickio_app.h"
#include "../lib/quick-event/src/qev.h"
#include "../lib/http-parser/http_parser.h"
#include "config.h"
#include "evs.h"
#include "evs_qio.h"
#include "evs_query.h"
#include "protocols.h"
#include "protocols/flash.h"
#include "protocols/raw.h"
#include "protocols/rfc6455.h"
#include "protocols/stomp.h"

struct client {
	/**
	 * All for quick-event
	 */
	struct qev_client qev_client;

	/**
	 * The state of the client at the protocol level. Protocols may only
	 * touch this once they have signaled that they handle the client.
	 *
	 * @note Protocols may only use bits 0-14, bit 15 (identified by 0x8000) is
	 * reserved for the protocol router.
	 */
	guint16 protocol_flags;

	/**
	 * Once a protocol has accepted the client, this will point to that
	 * protocol's routing function.
	 */
	struct protocol *protocol;

	/**
	 * The only timeout that will be set on a client from QIO.
	 *
	 * @attention This MAY ONLY ever be set on connect and in the protocols.
	 * It may never be altered anywhere else.
	 */
	qev_timeout_t *timeout;
};

/**
 * Extends the reasons for closing a client
 */
enum quickio_close_reasons {
	/**
	 * This client doesn't speak any language we know
	 */
	QIO_CLOSE_NOT_SUPPORTED = QEV_CLOSE_LEN + 1,

	/**
	 * The error that happens when a protocol returns FATAL on a client and
	 * didn't close the client before returning.
	 */
	QIO_CLOSE_UNKNOWN_ERROR,

	/**
	 * This client didn't finish his handshake properly
	 */
	QIO_CLOSE_INVALID_HANDSHAKE,

	/**
	 * Our interactions with this client are complete. Nothing
	 * remains to be said
	 */
	QIO_CLOSE_OK,
};

/**
 * Run the server from anywhere.
 */
void qio_main(int argc, char **argv);
