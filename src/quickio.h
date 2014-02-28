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

#ifndef G_LOG_DOMAIN
	/**
	 * We log to quickio. Apps should override to log to their own names.
	 */
	#define G_LOG_DOMAIN "quickio"
#endif

/**
 * Our client is of type `struct client`
 */
#define QEV_CLIENT client

#ifdef QIO_SERVER
	#include "quick-event/src/qev.h"
#else
	#include "quick-event/qev.h"
#endif

#include <gmodule.h>
#include "atomic.h"
#include "evs.h"
#include "apps.h"
#include "client.h"
#include "config.h"
#include "evs_qio.h"
#include "evs_query.h"
#include "str.h"
#include "sub.h"
#include "protocols.h"
#include "protocols/flash.h"
#include "protocols/raw.h"
#include "protocols/rfc6455.h"
#include "protocols/stomp.h"
#include "protocols/util.h"

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
	QIO_CLOSE_DONE,
};

/**
 * Run the server from anywhere.
 */
void qio_main(int argc, char **argv);
