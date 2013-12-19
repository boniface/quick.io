/**
 * Every configuration option that QIO has.
 * @file config.h
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
 * The maximum number of subscriptions a single client may have.
 */
guint64 cfg_max_subscriptions;

/**
 * Address to bind to for plaintext connections.
 */
gchar *cfg_bind_address;

/**
 * The port to bind to for plaintext connections.
 */
guint64 cfg_bind_port;

/**
 * Address to bind to for SSL connections.
 */
gchar *cfg_bind_address_ssl;

/**
 * The port to bind to for SSL connections.
 */
guint64 cfg_bind_port_ssl;

/**
 * Number of threads used to pump out each broadcasted message. These threads
 * come from the qev_pool of threads.
 */
guint64 cfg_broadcast_threads;

/**
 * The first certificate to use for the SSL connection. This MUST be a full
 * certificate chain in PEM format, starting from _your_ certificate on top,
 * all intermediaries, then the root CA.
 *
 * @note Typically RSA.
 */
gchar *cfg_ssl_cert_path_0;

/**
 * The key that corresponds to certificate 0.
 */
gchar *cfg_ssl_key_path_0;

/**
 * Same thing as cfg_ssl_cert_path_0, but for an ECDSA key.
 */
gchar *cfg_ssl_cert_path_1;

/**
 * The key that corresponds to certificate 1.
 */
gchar *cfg_ssl_key_path_1;

/**
 * Registers all configuration options with QEV.
 */
void config_init();
